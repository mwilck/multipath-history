#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <linux/unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libdevmapper.h>
#include <signal.h>
#include <wait.h>
#include <sched.h>
#include <errno.h>
#include <syslog.h>
#include <sys/mount.h>
#include <sys/mman.h>

/*
 * libsysfs
 */
#include <sysfs/libsysfs.h>
#include <sysfs/dlist.h>

/*
 * libcheckers
 */
#include <checkers.h>
#include <path_state.h>

/*
 * libmultipath
 */
#include <parser.h>
#include <vector.h>
#include <memory.h>
#include <callout.h>
#include <safe_printf.h>

#include "hwtable.h"
#include "dict.h"
#include "devinfo.h"
#include "copy.h"
#include "clone_platform.h"

#define MAXPATHS 1024
#define MAXMAPS 256
#define FILENAMESIZE 256
#define MAPNAMESIZE 64
#define TARGETTYPESIZE 16
#define PARAMSSIZE 2048
#define DEV_T_SIZE 32

#define PIDFILE "/var/run/multipathd.pid"
#define CONFIGFILE "/etc/multipath.conf"
#define CALLOUT_DIR "/var/cache/multipathd"

#ifndef LOGLEVEL
#define LOGLEVEL 5
#endif

#define MATCH(x, y) strncmp(x, y, strlen(x)) == 0
#define LOG_MSG(a,b) \
	if (strlen(a)) { \
		syslog(LOG_WARNING, "%s: %s", b, a); \
		memset(a, 0, MAX_CHECKER_MSG_SIZE); \
	}

/*
 * global vars
 */
int from_sighup;
pthread_mutex_t *event_lock;
pthread_cond_t *event;

/*
 * structs
 */
struct path {
	char dev_t[DEV_T_SIZE];
	int state;
	int (*checkfn) (char *, char *, void *);
	void * checker_context;
};

struct paths {
	pthread_mutex_t *lock;
	vector pathvec;
};

struct event_thread {
	pthread_t *thread;
	pthread_mutex_t *waiter_lock;
	int event_nr;
	char mapname[MAPNAMESIZE];
};

/*
 * helpers functions
 */
static int
blacklist (char * dev) {
	int i;
	char *p;

	vector_foreach_slot (blist, p, i)
		if (memcmp(dev, p, strlen(p)) == 0)
			return 1;
	return 0;
}

static void
strvec_free (vector vec)
{
	int i;
	char * str;

	vector_foreach_slot (vec, str, i)
		if (str)
			FREE(str);

	vector_free(vec);
}

static int
select_checkfn(struct path *pp, char *devname)
{
	char vendor[8];
	char product[16];
	char rev[4];
	char checker_name[CHECKER_NAME_SIZE];
	int i, r;
	struct hwentry * hwe;

	/*
	 * default checkfn
	 */
	pp->checkfn = &readsector0;
	
	r = get_lun_strings(vendor, product, rev, devname);

	if (r) {
		syslog(LOG_ERR, "can not get scsi strings");
		return r;
	}

	vector_foreach_slot (hwtable, hwe, i) {
		if (MATCH(hwe->vendor, vendor) &&
		    MATCH(hwe->product, product) &&
		    hwe->checker_index > 0) {
			get_checker_name(checker_name, hwe->checker_index);
			syslog(LOG_INFO, "set %s path checker for %s",
			     checker_name, devname);
			pp->checkfn = get_checker_addr(hwe->checker_index);
			return 0;
		}
	}
	syslog(LOG_INFO, "set readsector0 path checker for %s", devname);
	return 0;
}

static int
exit_daemon (int status)
{
	if (status != 0)
		fprintf(stderr, "bad exit status. see daemon.log\n");

	syslog(LOG_INFO, "umount ramfs");
	umount(CALLOUT_DIR);

	syslog(LOG_INFO, "unlink pidfile");
	unlink(PIDFILE);

	syslog(LOG_NOTICE, "--------shut down-------");
	exit(status);
}

static void *
get_devmaps (void)
{
	char *devmap;
	struct dm_task *dmt, *dmt1;
	struct dm_names *names = NULL;
	unsigned next = 0;
	void *nexttgt;
	uint64_t start, length;
	char *target_type = NULL;
	char *params;
	vector devmaps;

	devmaps = vector_alloc();

	if (!(dmt = dm_task_create(DM_DEVICE_LIST)))
		return NULL;

	if (!dm_task_run(dmt)) {
		devmaps = NULL;
		goto out;
	}

	if (!(names = dm_task_get_names(dmt))) {
		devmaps = NULL;
		goto out;
	}

	if (!names->dev) {
		syslog(LOG_WARNING, "no devmap found");
		goto out;
	}

	do {
		/*
		 * keep only multipath maps
		 */
		names = (void *) names + next;
		nexttgt = NULL;
		syslog(LOG_DEBUG, "devmap %s :", names->name);

		if (!(dmt1 = dm_task_create(DM_DEVICE_STATUS)))
			goto out;
		
		if (!dm_task_set_name(dmt1, names->name))
			goto out1;
		
		if (!dm_task_run(dmt1))
			goto out1;

		do {
			nexttgt = dm_get_next_target(dmt1, nexttgt,
						   &start,
						   &length,
						   &target_type,
						   &params);
			syslog(LOG_DEBUG, "\\_ %lu %lu %s", (unsigned long) start,
						  (unsigned long) length,
						  target_type);

			if (!target_type) {
				syslog(LOG_INFO, "   unknown target type");
				goto out1;
			}
			
			if (!strncmp(target_type, "multipath", 9)) {
				devmap = MALLOC(MAPNAMESIZE);
				strcpy(devmap, names->name);
				vector_alloc_slot(devmaps);
				vector_set_slot(devmaps, devmap);
			} else
				syslog(LOG_DEBUG, "   skip non multipath target");
		} while (nexttgt);

out1:
		dm_task_destroy(dmt1);
		next = names->next;

	} while (next);

out:
	dm_task_destroy(dmt);
	return devmaps;
}

static int
updatepaths (struct paths *allpaths)
{
	int i;
	struct path *pp;
	struct sysfs_directory * sdir;
	struct sysfs_directory * devp;
	char path[FILENAMESIZE];
	char sysfs_path[FILENAMESIZE];
	char attr_path[FILENAMESIZE];
	char attr_buff[17];
	
	if (sysfs_get_mnt_path(sysfs_path, FILENAMESIZE)) {
		syslog(LOG_ERR, "can not find sysfs mount point");
		return 1;
	}

	if (safe_sprintf(path, "%s/block", sysfs_path)) {
		fprintf(stderr, "updatepaths: path too small\n");
		return 1;
	}
	sdir = sysfs_open_directory(path);
	sysfs_read_dir_subdirs(sdir);
	pthread_mutex_lock(allpaths->lock);

	dlist_for_each_data(sdir->subdirs, devp, struct sysfs_directory) {
		if (blacklist(devp->name)) {
			syslog(LOG_DEBUG, "%s blacklisted", devp->name);
			continue;
		}
		memset(attr_buff, 0, sizeof (attr_buff));
		memset(attr_path, 0, sizeof (attr_path));

		if (safe_sprintf(attr_path, "%s/block/%s/dev",
			sysfs_path, devp->name)) {
			fprintf(stderr, "updatepaths: attr_path too small\n");
			continue;
		}
		if (0 > sysfs_read_attribute_value(attr_path, attr_buff, 17)) {
			syslog(LOG_DEBUG, "no such attribute : %s",
				attr_path);
			continue;
		}
		/*
		 * detect if path already exists in path vector
		 * if so, keep the old one for for checker context and
		 * state persistance
		 */
		vector_foreach_slot (allpaths->pathvec, pp, i)
			if (0 == strncmp(pp->dev_t, attr_buff,
					strlen(pp->dev_t)))
				break;

		if (i < VECTOR_SIZE(allpaths->pathvec)) {
			syslog(LOG_INFO, "path checker already active : %s",
				pp->dev_t);
			continue;
		}

		/*
		 * ok, really allocate a path
		 */
		pp = MALLOC(sizeof(struct path));

		if (safe_snprintf(pp->dev_t, DEV_T_SIZE, "%s",
				  attr_buff)) {
			fprintf(stderr, "dev_t too small\n");
			FREE(pp);
			continue;
		}
		pp->checker_context = MALLOC(MAX_CHECKER_CONTEXT_SIZE);

		if (select_checkfn(pp, devp->name)) {
			FREE(pp->checker_context);
			FREE(pp);
			continue;
		}
		pp->state = checkpath(pp->dev_t, pp->checkfn, NULL,
					pp->checker_context);

		vector_alloc_slot(allpaths->pathvec);
		vector_set_slot(allpaths->pathvec, pp);

		syslog(LOG_NOTICE, "path checker startup : %s", pp->dev_t);
	}
	pthread_mutex_unlock(allpaths->lock);
	sysfs_close_directory(sdir);
	return 0;
}

static int
geteventnr (char *name)
{
	struct dm_task *dmt;
	struct dm_info info;
	
	if (!(dmt = dm_task_create(DM_DEVICE_INFO)))
		return 0;

	if (!dm_task_set_name(dmt, name))
		goto out;

	if (!dm_task_run(dmt))
		goto out;

	if (!dm_task_get_info(dmt, &info)) {
		info.event_nr = 0;
		goto out;
	}

	if (!info.exists) {
		syslog(LOG_ERR, "Device %s does not exist", name);
		info.event_nr = 0;
		goto out;
	}

out:
	dm_task_destroy(dmt);

	return info.event_nr;
}

static void *
waitevent (void * et)
{
	struct event_thread *waiter;
	char buff[1];
	char * bin;

	mlockall(MCL_CURRENT | MCL_FUTURE);
	waiter = (struct event_thread *)et;
	pthread_mutex_lock (waiter->waiter_lock);

	waiter->event_nr = geteventnr (waiter->mapname);
	syslog(LOG_DEBUG, "waiter->event_nr = %i", waiter->event_nr);

	struct dm_task *dmt;

	if (!(dmt = dm_task_create(DM_DEVICE_WAITEVENT)))
		return 0;

	if (!dm_task_set_name(dmt, waiter->mapname))
		goto out;

	if (waiter->event_nr && !dm_task_set_event_nr(dmt, waiter->event_nr))
		goto out;

	dm_task_run(dmt);

out:
	dm_task_destroy(dmt);

	bin = MALLOC(strlen(multipath) + WWID_SIZE + 1);
	sprintf(bin, "%s %s", multipath, waiter->mapname);

	syslog(LOG_DEBUG, "%s", bin);
	syslog(LOG_NOTICE, "devmap event on %s", waiter->mapname);
	syslog(LOG_INFO, "%s : reconfigure multipath map",
			waiter->mapname);
	execute_program(multipath, buff, 1);

	/*
	 * tell waiterloop we have an event
	 */
	pthread_mutex_lock (event_lock);
	pthread_cond_signal(event);
	pthread_mutex_unlock (event_lock);
	
	/*
	 * release waiter_lock so that waiterloop knows we are gone
	 */
	pthread_mutex_unlock (waiter->waiter_lock);
	pthread_exit(waiter->thread);

	return (NULL);
}

static void *
waiterloop (void *ap)
{
	struct paths * allpaths;
	vector devmaps = NULL;
	char * devmap;
	vector waiters;
	struct event_thread *wp;
	pthread_attr_t attr;
	int r;
	char buff[1];
	int i, j;

	syslog(LOG_NOTICE, "start DM events thread");
	mlockall(MCL_CURRENT | MCL_FUTURE);

	/*
	 * inits
	 */
	allpaths = (struct paths *)ap;
	waiters = vector_alloc();
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 32 * 1024);

	syslog(LOG_NOTICE, "initial reconfigure multipath maps");
	execute_program(multipath, buff, 1);

	while (1) {
		/*
		 * update devmap list
		 */
		syslog(LOG_INFO, "refresh devmaps list");
		if (devmaps != NULL)
			strvec_free(devmaps);

		while ((devmaps = get_devmaps()) == NULL) {
			/*
			 * we're not allowed to fail here
			 */
			syslog(LOG_ERR, "can't get devmaps ... retry later");
			sleep(5);
		}

		/*
		 * update paths list
		 */
		syslog(LOG_INFO, "refresh failpaths list");
		updatepaths(allpaths);

		/*
		 * start waiters on all devmaps
		 */
		syslog(LOG_INFO, "start up event loops");

		vector_foreach_slot (devmaps, devmap, i) {
			/*
			 * find out if devmap already has
			 * a running waiter thread
			 */
			vector_foreach_slot (waiters, wp, j)
				if (!strcmp (wp->mapname, devmap))
					break;
					
			/*
			 * no event_thread struct : init it
			 */
			if (j == VECTOR_SIZE(waiters)) {
				wp = MALLOC (sizeof (struct event_thread));
				strcpy (wp->mapname, devmap);
				wp->thread = MALLOC (sizeof (pthread_t));
				wp->waiter_lock = (pthread_mutex_t *) 
					MALLOC (sizeof (pthread_mutex_t));
				pthread_mutex_init (wp->waiter_lock, NULL);
				vector_alloc_slot (waiters);
				vector_set_slot (waiters, wp);
			}
			
			/*
			 * event_thread struct found
			 */
			if (j < VECTOR_SIZE(waiters)) {
				r = pthread_mutex_trylock (wp->waiter_lock);

				/*
				 * thread already running : next devmap
				 */
				if (r)
					continue;
				
				pthread_mutex_unlock (wp->waiter_lock);
			}
			
			syslog(LOG_NOTICE, "event checker startup : %s", wp->mapname);
			pthread_create (wp->thread, &attr, waitevent, wp);
			pthread_detach (*wp->thread);
		}
		/*
		 * wait event condition
		 */
		pthread_mutex_lock (event_lock);
		pthread_cond_wait(event, event_lock);
		pthread_mutex_unlock (event_lock);
	}

	return (NULL);
}

static void *
checkerloop (void *ap)
{
	struct paths *allpaths;
	struct path *pp;
	int i;
	int newstate;
	char buff[1];
	char * bin;
	char checker_msg[MAX_CHECKER_MSG_SIZE];

	memset(checker_msg, 0, MAX_CHECKER_MSG_SIZE);

	mlockall(MCL_CURRENT | MCL_FUTURE);
	allpaths = (struct paths *)ap;

	syslog(LOG_NOTICE, "path checkers start up");

	while (1) {
		pthread_mutex_lock(allpaths->lock);
		syslog(LOG_DEBUG, "checking paths");

		vector_foreach_slot (allpaths->pathvec, pp, i) {
			newstate = checkpath(pp->dev_t, pp->checkfn,
				      checker_msg, pp->checker_context);
			
			if (newstate != pp->state) {
				pp->state = newstate;
				LOG_MSG(checker_msg, pp->dev_t);

				/*
				 * don't trigger map reconfiguration for
				 * path going down. It will be handled
				 * in due time by DM event waiters
				 */
				if (newstate == PATH_DOWN ||
				    newstate == PATH_SHAKY)
					continue;

				/*
				 * reconfigure map now
				 */
				bin = MALLOC(strlen(multipath) + DEV_T_SIZE);
				sprintf(bin, "%s -D %s", multipath, pp->dev_t);

				syslog(LOG_DEBUG, "%s", bin);
				syslog(LOG_INFO, "reconfigure multipath map");
				execute_program(bin, buff, 1);

				/*
				 * tell waiterloop we have an event
				 */
				pthread_mutex_lock (event_lock);
				pthread_cond_signal(event);
				pthread_mutex_unlock (event_lock);
			}
			pp->state = newstate;
		}
		pthread_mutex_unlock(allpaths->lock);
		sleep(checkint);
	}

	return (NULL);
}

static struct paths *
initpaths (void)
{
	struct paths *allpaths;

	allpaths = MALLOC (sizeof (struct paths));
	allpaths->lock = 
		(pthread_mutex_t *) MALLOC (sizeof (pthread_mutex_t));
	allpaths->pathvec = vector_alloc();
	pthread_mutex_init (allpaths->lock, NULL);

	event = (pthread_cond_t *) MALLOC (sizeof (pthread_cond_t));
	pthread_cond_init (event, NULL);
	event_lock = (pthread_mutex_t *) MALLOC (sizeof (pthread_mutex_t));
	pthread_mutex_init (event_lock, NULL);
	
	return (allpaths);
}

#define VECTOR_ADDSTR(a, b) \
	str = MALLOC (6 * sizeof(char)); \
	snprintf(str, 6, b); \
	vector_alloc_slot(a); \
	vector_set_slot(a, str);

static void
setup_default_blist (vector blist)
{
	char * str;

	VECTOR_ADDSTR(blist, "cciss");
	VECTOR_ADDSTR(blist, "fd");
	VECTOR_ADDSTR(blist, "hd");
	VECTOR_ADDSTR(blist, "md");
	VECTOR_ADDSTR(blist, "dm");
	VECTOR_ADDSTR(blist, "sr");
	VECTOR_ADDSTR(blist, "scd");
	VECTOR_ADDSTR(blist, "st");
	VECTOR_ADDSTR(blist, "ram");
	VECTOR_ADDSTR(blist, "raw");
	VECTOR_ADDSTR(blist, "loop");
}

#define VECTOR_ADDHWE(a, b, c, d) \
	hwe = MALLOC (sizeof(struct hwentry)); \
	hwe->vendor = MALLOC (9 * sizeof(char)); \
	snprintf (hwe->vendor, 9, b); \
	hwe->product = MALLOC (17 * sizeof(char)); \
	snprintf (hwe->product, 17, c); \
	hwe->checker_index = d; \
	vector_alloc_slot(a); \
	vector_set_slot(a, hwe);

static void
setup_default_hwtable (vector hwtable)
{
	struct hwentry * hwe;

	VECTOR_ADDHWE(hwtable, "COMPAQ", "HSV110 (C)COMPAQ", 0);
	VECTOR_ADDHWE(hwtable, "COMPAQ", "MSA1000", 0);
	VECTOR_ADDHWE(hwtable, "COMPAQ", "MSA1000 VOLUME", 0);
	VECTOR_ADDHWE(hwtable, "DEC", "HSG80", 0);
	VECTOR_ADDHWE(hwtable, "HP", "HSV110", 0);
	VECTOR_ADDHWE(hwtable, "HP", "A6189A", 0);
	VECTOR_ADDHWE(hwtable, "HP", "OPEN-", 0);
	VECTOR_ADDHWE(hwtable, "DDN", "SAN DataDirector", 0);
	VECTOR_ADDHWE(hwtable, "FSC", "CentricStor", 0);
	VECTOR_ADDHWE(hwtable, "HITACHI", "DF400", 0);
	VECTOR_ADDHWE(hwtable, "HITACHI", "DF500", 0);
	VECTOR_ADDHWE(hwtable, "HITACHI", "DF600", 0);
	VECTOR_ADDHWE(hwtable, "IBM", "ProFibre 4000R", 0);
	VECTOR_ADDHWE(hwtable, "SGI", "TP9100", 0);
	VECTOR_ADDHWE(hwtable, "SGI", "TP9300", 0);
	VECTOR_ADDHWE(hwtable, "SGI", "TP9400", 0);
	VECTOR_ADDHWE(hwtable, "SGI", "TP9500", 0);
	VECTOR_ADDHWE(hwtable, "3PARdata", "VV", 0);
	VECTOR_ADDHWE(hwtable, "STK", "OPENstorage D280", 0);
	VECTOR_ADDHWE(hwtable, "SUN", "StorEdge 3510", 0);
	VECTOR_ADDHWE(hwtable, "SUN", "T4", 0);
}

/*
 * this logic is all about keeping callouts working in case of
 * system disk outage (think system over SAN)
 * this needs the clone syscall, so don't bother if not present
 * (Debian Woody)
 */
#ifdef CLONE_NEWNS
static int
prepare_namespace(void)
{
	mode_t mode;
	struct stat *buf;
	char ramfs_args[64];
	int i;
	int fd;
	char * bin;
	size_t size = 10;
	struct stat statbuf;
	
	buf = MALLOC(sizeof(struct stat));

	/*
	 * create a temp mount point for ramfs
	 */
	if (stat (CALLOUT_DIR, buf) < 0) {
		if (mkdir(CALLOUT_DIR, mode) < 0) {
			syslog(LOG_ERR, "cannot create " CALLOUT_DIR);
			return -1;
		}
		syslog(LOG_DEBUG, "created " CALLOUT_DIR);
	}

	/*
	 * compute the optimal ramdisk size
	 */
	vector_foreach_slot (binvec, bin,i) {
		if ((fd = open(bin, O_RDONLY)) < 0) {
			syslog(LOG_ERR, "cannot open %s", bin);
			return -1;
		}
		if (fstat(fd, &statbuf) < 0) {
			syslog(LOG_ERR, "cannot stat %s", bin);
			return -1;
		}
		size += statbuf.st_size;
		close(fd);
	}
	syslog(LOG_INFO, "ramfs maxsize is %u", (unsigned int) size);
	
	/*
	 * mount the ramfs
	 */
	if (safe_sprintf(ramfs_args, "maxsize=%u", (unsigned int) size)) {
		fprintf(stderr, "ramfs_args too small\n");
		return -1;
	}
	if (mount(NULL, CALLOUT_DIR, "ramfs", MS_SYNCHRONOUS, ramfs_args) < 0) {
		syslog(LOG_ERR, "cannot mount ramfs on " CALLOUT_DIR);
		return -1;
	}
	syslog(LOG_DEBUG, "mount ramfs on " CALLOUT_DIR);

	/*
	 * populate the ramfs with callout binaries
	 */
	vector_foreach_slot (binvec, bin,i) {
		if (copytodir(bin, CALLOUT_DIR) < 0) {
			syslog(LOG_ERR, "cannot copy %s in ramfs", bin);
			exit_daemon(1);
		}
		syslog(LOG_DEBUG, "cp %s in ramfs", bin);
	}
	strvec_free(binvec);

	/*
	 * bind the ramfs to :
	 * /sbin : default home of multipath ...
	 * /bin  : default home of scsi_id ...
	 * /tmp  : home of tools temp files
	 */
	if (mount(CALLOUT_DIR, "/sbin", NULL, MS_BIND, NULL) < 0) {
		syslog(LOG_ERR, "cannot bind ramfs on /sbin");
		return -1;
	}
	syslog(LOG_DEBUG, "bind ramfs on /sbin");
	if (mount(CALLOUT_DIR, "/bin", NULL, MS_BIND, NULL) < 0) {
		syslog(LOG_ERR, "cannot bind ramfs on /bin");
		return -1;
	}
	syslog(LOG_DEBUG, "bind ramfs on /bin");
	if (mount(CALLOUT_DIR, "/tmp", NULL, MS_BIND, NULL) < 0) {
		syslog(LOG_ERR, "cannot bind ramfs on /tmp");
		return -1;
	}
	syslog(LOG_DEBUG, "bind ramfs on /tmp");

	return 0;
}
#endif

static void
pidfile (pid_t pid)
{
	FILE *file;
	struct stat *buf;

	buf = MALLOC(sizeof(struct stat));

	if (!stat(PIDFILE, buf)) {
		syslog(LOG_ERR, "already running : out");
		FREE(buf);
		exit(1);
	}
		
	umask(022);
	pid = setsid();

	if (pid < -1) {
		syslog(LOG_ERR, "setsid() error");
		exit(1);
	}
	
	file = fopen(PIDFILE, "w");
	fprintf(file, "%d\n", pid);
	fclose(file);
	FREE(buf);
}

static void *
signal_set(int signo, void (*func) (int))
{
	int r;
	struct sigaction sig;
	struct sigaction osig;

	sig.sa_handler = func;
	sigemptyset(&sig.sa_mask);
	sig.sa_flags = 0;

	r = sigaction(signo, &sig, &osig);

	if (r < 0)
		return (SIG_ERR);
	else
		return (osig.sa_handler);
}

static void
sighup (int sig)
{
	syslog(LOG_NOTICE, "SIGHUP received from multipath or operator");

	/*
	 * signal updatepaths() that we come from SIGHUP
	 */
	from_sighup = 1;

	/*
	 * ask for allpaths refresh
	 */
	pthread_mutex_lock (event_lock);
	pthread_cond_signal(event);
	pthread_mutex_unlock (event_lock);
}

static void
sigend (int sig)
{
	exit_daemon(0);
}

static void
signal_init(void)
{
	signal_set(SIGHUP, sighup);
	signal_set(SIGINT, sigend);
	signal_set(SIGTERM, sigend);
	signal_set(SIGKILL, sigend);
}

static void
setscheduler (void)
{
        int res;
	static struct sched_param sched_param = {
		sched_priority: 99
	};

        res = sched_setscheduler (0, SCHED_RR, &sched_param);

        if (res == -1)
                syslog(LOG_WARNING, "Could not set SCHED_RR at priority 99");
	return;
}

static int
child (void * param)
{
	pthread_t wait_thr, check_thr;
	pthread_attr_t attr;
	struct paths *allpaths;

	openlog("multipathd", 0, LOG_DAEMON);
	setlogmask(LOG_UPTO(LOGLEVEL));
	syslog(LOG_NOTICE, "--------start up--------");

	pidfile(getpid());
	signal_init();
	setscheduler();
	allpaths = initpaths();
	checkint = CHECKINT;

	syslog(LOG_INFO, "read " CONFIGFILE);
	init_data(CONFIGFILE, init_keywords);

	/*
	 * fill the voids left in the config file
	 */
	if (binvec == NULL)
		binvec = vector_alloc();

	if (multipath == NULL) {
		multipath = MULTIPATH;
		push_callout(multipath);
	}

	if (hwtable == NULL) {
		hwtable = vector_alloc();
		setup_default_hwtable(hwtable);
	}

	if (blist == NULL) {
		blist = vector_alloc();
		setup_default_blist(blist);
	}

#ifdef CLONE_NEWNS
	if (prepare_namespace() < 0) {
		syslog(LOG_ERR, "cannot prepare namespace");
		exit_daemon(1);
	}
#endif

	mlockall(MCL_CURRENT | MCL_FUTURE);
	/*
	 * start threads
	 */
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 64 * 1024);
	
	pthread_create(&wait_thr, &attr, waiterloop, allpaths);
	pthread_create(&check_thr, &attr, checkerloop, allpaths);
	pthread_join(wait_thr, NULL);
	pthread_join(check_thr, NULL);

	return 0;
}

int
main (int argc, char *argv[])
{
	int err;
	void * child_stack = (void *)malloc(CHILD_STACK_SIZE);

#ifdef CLONE_NEWNS	/* recent systems have clone() */

#    if defined(__hppa__) || defined(__powerpc64__)
	err = clone(child, child_stack, CLONE_NEWNS, NULL);
#    elif defined(__ia64__)
	err = clone2(child, child_stack,
		     CHILD_STACK_SIZE, CLONE_NEWNS, NULL,
		     NULL, NULL, NULL);
#    else
	err = clone(child, child_stack + CHILD_STACK_SIZE, CLONE_NEWNS, NULL);
#    endif
	if (err < 0)
		exit (1);

	exit(0);
#else			/* older system fallback to fork() */
	err = fork();
	
	if (err < 0)
		exit (1);

	return (child(child_stack));
#endif

}
