#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libdevmapper.h>
#include <syslog.h>
#include <signal.h>
#include <wait.h>
#include <sched.h>
#include <asm/unistd.h>
#include <errno.h>
#include <sys/mount.h>
#include <sys/mman.h>
#include "libsysfs/sysfs/libsysfs.h"
#include "libsysfs/dlist.h"

#include "hwtable.h"
#include "dict.h"
#include "parser.h"
#include "vector.h"
#include "devinfo.h"
#include "checkers.h"
#include "memory.h"
#include "copy.h"

#define MAXPATHS 1024
#define MAXMAPS 256
#define FILENAMESIZE 256
#define MAPNAMESIZE 64
#define TARGETTYPESIZE 16
#define PARAMSSIZE 2048

#define MULTIPATH "/sbin/multipath"
#define PIDFILE "/var/run/multipathd.pid"
#define CONFIGFILE "/etc/multipath.conf"
#define CALLOUT_DIR "/var/cache/multipathd"

#ifndef DEBUG
#define DEBUG 1
#endif
#define LOG(x, y, z...) if (DEBUG>=x) syslog(x, "[%lu] " y, pthread_self(), ##z)
#define MATCH(x, y) strncmp(x, y, strlen(y)) == 0

/*
 * for sys_clone use
 */
#define __NR_sys_clone __NR_clone 
static inline _syscall2(int,sys_clone, int,flag, void*,stack);

/*
 * global vars
 */
int from_sighup;
pthread_mutex_t *event_lock;
pthread_cond_t *event;

/*
 * structs
 */
struct path
{
	int major;
	int minor;
	int (*checkfn) (char *);
};

struct paths
{
	pthread_mutex_t *lock;
	vector pathvec;
};

struct event_thread
{
	pthread_t *thread;
	pthread_mutex_t *waiter_lock;
	int event_nr;
	char mapname[MAPNAMESIZE];
};

/*
 * helpers functions
 */
static int
makenode (char *devnode, int major, int minor)
{
	dev_t dev;
	
	dev = makedev (major, minor);
        unlink (devnode);
	
	return mknod(devnode, S_IFCHR | S_IRUSR | S_IWUSR, dev);
}

static int
blacklist (char * dev) {
	int i;
	char *p;

	for (i = 0; i < VECTOR_SIZE(blist); i++) {
		p = VECTOR_SLOT(blist, i);
		
		if (memcmp(dev, p, strlen(p)) == 0)
			return 1;
	}
	return 0;
}

static void
strvec_free (vector vec)
{
	int i;
	char * str;

	for (i =0; i < VECTOR_SIZE(vec); i++)
		if ((str = VECTOR_SLOT(vec, i)) != NULL) {
			free(str);
		}
	vector_free(vec);
}

static void
pathvec_free (vector vec)
{
	int i;
	struct path * pp;

	for (i =0; i < VECTOR_SIZE(vec); i++)
		if ((pp = VECTOR_SLOT(vec, i)) != NULL)
			free(pp);
	vector_free(vec);
}

static int
select_checkfn(struct path *path_p, char *devname)
{
	char vendor[8];
	char product[16];
	char rev[4];
	int i, r;
	struct hwentry * hwe;

	/*
	 * default checkfn
	 */
	path_p->checkfn = &readsector0;
	
	r = get_lun_strings(vendor, product, rev, devname);

	if (r) {
		LOG(1, "can not get device strings");
		return r;
	}

	for (i = 0; i < VECTOR_SIZE(hwtable); i++) {
		hwe = VECTOR_SLOT(hwtable, i);
		if (MATCH(hwe->vendor, vendor) &&
		    MATCH(hwe->product, product)) {
			LOG (2, "set %s path checker for %s",
			     checker_list[hwe->checker_index].name,
			     devname);
			path_p->checkfn = checker_list[hwe->checker_index].checker;
		}
	}

	return 0;
}

static int
exit_daemon (int status)
{
	if (status != 0)
		fprintf(stderr, "bad exit status. see daemon.log\n");
	LOG (2, "umount ramfs");
	umount(CALLOUT_DIR);
	LOG (2, "unlink pidfile");
	unlink (PIDFILE);
	LOG (1, "--------shut down-------");
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
		LOG (1, "no devmap found");
		goto out;
	}

	do {
		/*
		 * keep only multipath maps
		 */
		names = (void *) names + next;
		nexttgt = NULL;
		LOG (3, "iterate on devmap names : %s", names->name);

		if (!(dmt1 = dm_task_create(DM_DEVICE_STATUS)))
			goto out;
		
		if (!dm_task_set_name(dmt1, names->name))
			goto out1;
		
		if (!dm_task_run(dmt1))
			goto out1;

		LOG (3, "DM_DEVICE_STATUS ioctl done");
		do {
			LOG (3, "iterate on devmap's targets");
			nexttgt = dm_get_next_target(dmt1, nexttgt,
						   &start,
						   &length,
						   &target_type,
						   &params);

			LOG (3, "test target_type existence");
			if (!target_type)
				goto out1;
			
			LOG (3, "test if target_type is multipath");
			if (!strncmp (target_type, "multipath", 9)) {
				devmap = malloc (MAPNAMESIZE);
				strcpy (devmap, names->name);
				vector_alloc_slot (devmaps);
				vector_set_slot (devmaps, devmap);
			}
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
checkpath (struct path *path_p)
{
	char devnode[FILENAMESIZE];
	int r;
	
	sprintf (devnode, "/tmp/.checker.%i.%i", path_p->major, path_p->minor);
	r = makenode (devnode, path_p->major, path_p->minor);

	if (r < 0) {
		LOG (2, "can not create %s", devnode);
		return r;
	}

	r = path_p->checkfn(devnode);
	unlink (devnode);
				
	LOG (2, "checked path %i:%i => %s",
	     path_p->major, path_p->minor,
	     r ? "up" : "down");

	return r;
}

static int
updatepaths (struct paths *failedpaths)
{
	struct path *path_p;
	struct sysfs_directory * sdir;
	struct sysfs_directory * devp;
	char path[FILENAMESIZE];
	char sysfs_path[FILENAMESIZE];
	char attr_path[FILENAMESIZE];
	char attr_buff[17];
	char word[5];
	char *p1, *p2;
	
	if (sysfs_get_mnt_path (sysfs_path, FILENAMESIZE)) {
		LOG (2, "can not find sysfs mount point");
		return 1;
	}

	sprintf (path, "%s/block", sysfs_path);
	sdir = sysfs_open_directory (path);
	sysfs_read_dir_subdirs (sdir);

	pthread_mutex_lock (failedpaths->lock);
	pathvec_free (failedpaths->pathvec);
	failedpaths->pathvec = vector_alloc();

	dlist_for_each_data (sdir->subdirs, devp, struct sysfs_directory) {
		if (blacklist (devp->name)) {
			LOG (3, "%s blacklisted", devp->name);
			continue;
		}

		memset (attr_buff, 0, sizeof (attr_buff));
		memset (attr_path, 0, sizeof (attr_path));
		sprintf(attr_path, "%s/block/%s/device/generic/dev",
			sysfs_path, devp->name);

		if (0 > sysfs_read_attribute_value(attr_path, attr_buff, 17)) {
			LOG (3, "no such attribute : %s",
				attr_path);
			continue;
		}

		path_p = malloc (sizeof (struct path));

		p1 = word;
		p2 = attr_buff;
		memset (word, 0, sizeof (word));
		
		while (*p2 != ':') {
			*p1 = *p2;
			p1++;
			p2++;
		}
		path_p->major = atoi (word);

		p1 = word;
		p2++;
		memset (&word, 0, sizeof (word));

		while (*p2 != ' ' && *p2 != '\0') {
			*p1 = *p2;
			p1++;
			p2++;
		}
		path_p->minor = atoi (word);

		if (!select_checkfn(path_p, devp->name) && checkpath(path_p)) {
			LOG(2, "discard %i:%i as valid path",
			    path_p->major, path_p->minor);
			free (path_p);
			continue;
		}

		vector_alloc_slot (failedpaths->pathvec);
		vector_set_slot (failedpaths->pathvec, path_p);

		LOG (2, "%i:%i added to failedpaths",
		     path_p->major, path_p->minor);
	}
	pthread_mutex_unlock (failedpaths->lock);
	sysfs_close_directory (sdir);
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
		LOG(1, "Device %s does not exist", name);
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

	mlockall(MCL_CURRENT | MCL_FUTURE);
	waiter = (struct event_thread *)et;
	pthread_mutex_lock (waiter->waiter_lock);

	waiter->event_nr = geteventnr (waiter->mapname);
	LOG (3, "waiter->event_nr = %i", waiter->event_nr);

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
	LOG (1, "devmap event on %s", waiter->mapname);

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
	struct paths *failedpaths;
	vector devmaps = NULL;
	char *devmap;
	vector waiters;
	struct event_thread *wp;
	pthread_attr_t attr;
	int r;
	char *cmdargs[4] = {MULTIPATH, "-q", "-S"};
	int i, j, status;

	mlockall(MCL_CURRENT | MCL_FUTURE);

	/*
	 * inits
	 */
	failedpaths = (struct paths *)ap;
	waiters = vector_alloc ();
	pthread_attr_init (&attr);
	pthread_attr_setstacksize (&attr, 32 * 1024);

	while (1) {
		/*
		 * upon waiter events and initial startup, do a preliminary
		 * multipath exec, no signal to avoid recursion.
		 * don't run multipath if we are waked from SIGHUP
		 * (oper exec / checkers / hotplug) because it already ran
		 */
		if (!from_sighup) {
			LOG (2, "exec multipath helper");
			if (fork () == 0)
				execve (cmdargs[0], cmdargs, NULL);
			wait (&status);
			LOG (2, "exec status = %s", status ? "failed" : "ok");
		} else
			from_sighup = 0;
		
		/*
		 * update devmap list
		 */
		LOG (2, "refresh devmaps list");
		if (devmaps != NULL)
			strvec_free(devmaps);

		while ((devmaps = get_devmaps()) == NULL) {
			/*
			 * we're not allowed to fail here
			 */
			LOG (1, "can't get devmaps ... retry later");
			sleep(5);
		}

		/*
		 * update failed paths list
		 */
		LOG (2, "refresh failpaths list");
		updatepaths (failedpaths);
		
		/*
		 * start waiters on all devmaps
		 */
		LOG (2, "start up event loops");

		for (i = 0; i < VECTOR_SIZE(devmaps); i++) {
			devmap = VECTOR_SLOT (devmaps, i);
			
			/*
			 * find out if devmap already has
			 * a running waiter thread
			 */
			for (j = 0; j < VECTOR_SIZE(waiters); j++) {
				wp = VECTOR_SLOT (waiters, j);

				if (!strcmp (wp->mapname, devmap))
					break;
			}
					
			/*
			 * no event_thread struct : init it
			 */
			if (j == VECTOR_SIZE(waiters)) {
				wp = zalloc (sizeof (struct event_thread));
				strcpy (wp->mapname, devmap);
				wp->thread = malloc (sizeof (pthread_t));
				wp->waiter_lock = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));
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
			
			LOG (1, "event checker startup : %s", wp->mapname);
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
	struct paths *failedpaths;
	struct path *path_p;
	int i;

	mlockall(MCL_CURRENT | MCL_FUTURE);
	failedpaths = (struct paths *)ap;

	LOG (1, "path checkers start up");

	while (1) {
		pthread_mutex_lock (failedpaths->lock);
		LOG (3, "checking paths");

		for (i = 0; i < VECTOR_SIZE(failedpaths->pathvec); i++) {
			path_p = VECTOR_SLOT (failedpaths->pathvec, i);
			
			if (checkpath (path_p)) {
				/*
				 * tell waiterloop we have an event
				 */
				LOG (1, "path checker event on device %i:%i",
					 path_p->major, path_p->minor);
				pthread_mutex_lock (event_lock);
				pthread_cond_signal(event);
				pthread_mutex_unlock (event_lock);
			}
		}
		pthread_mutex_unlock (failedpaths->lock);
		sleep (checkint ? checkint : CHECKINT);
	}

	return (NULL);
}

static struct paths *
initpaths (void)
{
	struct paths *failedpaths;

	failedpaths = malloc (sizeof (struct paths));
	failedpaths->lock = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));
	failedpaths->pathvec = vector_alloc();
	pthread_mutex_init (failedpaths->lock, NULL);
	event = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
	pthread_cond_init (event, NULL);
	event_lock = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));
	pthread_mutex_init (event_lock, NULL);
	
	return (failedpaths);
}

#define VECTOR_ADDSTR(a, b) \
	str = zalloc (6 * sizeof(char)); \
	sprintf (str, b); \
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
	hwe = zalloc (sizeof(struct hwentry)); \
	hwe->vendor = zalloc (9 * sizeof(char)); \
	sprintf (hwe->vendor, b); \
	hwe->product = zalloc (17 * sizeof(char)); \
	sprintf (hwe->product, c); \
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
 */
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
	
	buf = malloc (sizeof (struct stat));
	
	/*
	 * create a temp mount point for ramfs
	 */
	if (stat (CALLOUT_DIR, buf) < 0) {
		if (mkdir(CALLOUT_DIR, mode) < 0) {
			LOG(1, "cannot create " CALLOUT_DIR);
			return -1;
		}
		LOG(3, "created " CALLOUT_DIR);
	}

	/*
	 * compute the optimal ramdisk size (TODO)
	 */
	for (i = 0; i < VECTOR_SIZE(binvec); i++) {
		bin = VECTOR_SLOT(binvec, i);

		if ((fd = open(bin, O_RDONLY)) < 0) {
			LOG(1, "cannot open %s", bin);
			return -1;
		}
		if (fstat(fd, &statbuf) < 0) {
			LOG(1, "cannot stat %s", bin);
			return -1;
		}
		size += statbuf.st_size;
		close(fd);
	}
	if ((fd = open(MULTIPATH, O_RDONLY)) < 0) {
		LOG(1, "cannot open %s", bin);
		return -1;
	}
	if (fstat(fd, &statbuf) < 0) {
		LOG(1, "cannot stat %s", MULTIPATH);
		return -1;
	}
	size += statbuf.st_size;
	close(fd);

	LOG(3, "computed a ramfs maxsize of %i", size);
	
	/*
	 * mount the ramfs
	 */
	sprintf(ramfs_args, "maxsize=%i", size);
	if (mount(NULL, CALLOUT_DIR, "ramfs", MS_SYNCHRONOUS, ramfs_args) < 0) {
		LOG(1, "cannot mount ramfs on " CALLOUT_DIR);
		return -1;
	}
	LOG(3, "mounted ramfs on " CALLOUT_DIR);

	/*
	 * populate the ramfs with callout binaries
	 */
	for (i = 0; i < VECTOR_SIZE(binvec); i++) {
		bin = VECTOR_SLOT(binvec, i);
		
		if (copytodir(bin, CALLOUT_DIR) < 0) {
			LOG(1, "cannot copy %s in ramfs", bin);
			exit_daemon(1);
		}
		LOG(3, "copied %s in ramfs", bin);
	}
	if (copytodir(MULTIPATH, CALLOUT_DIR) < 0) {
		LOG(1, "cannot copy " MULTIPATH " in ramfs");
		exit_daemon(1);
	}
	LOG(3, "copied " MULTIPATH " in ramfs");

	/*
	 * bind the ramfs to :
	 * /sbin : home of multipath
	 * /bin  : home of scsi_id
	 * /tmp  : home of tools temp files
	 */
	if (mount(CALLOUT_DIR, "/sbin", NULL, MS_BIND, NULL) < 0) {
		LOG(1, "cannot bind ramfs on /sbin");
		return -1;
	}
	LOG(3, "bound ramfs on /sbin");
	if (mount(CALLOUT_DIR, "/bin", NULL, MS_BIND, NULL) < 0) {
		LOG(1, "cannot bind ramfs on /bin");
		return -1;
	}
	LOG(3, "bound ramfs on /bin");
	if (mount(CALLOUT_DIR, "/tmp", NULL, MS_BIND, NULL) < 0) {
		LOG(1, "cannot bind ramfs on /tmp");
		return -1;
	}
	LOG(3, "bound ramfs on /tmp");

	return 0;
}

static void
pidfile (pid_t pid)
{
	FILE *file;
	struct stat *buf;

	buf = malloc (sizeof (struct stat));

	if (!stat (PIDFILE, buf)) {
		LOG(1, "already running : out");
		free (buf);
		exit (1);
	}
		
	umask (022);
	pid = setsid ();

	if (pid < -1) {
		LOG(1, "setsid() error");
		exit (1);
	}
	
	file = fopen (PIDFILE, "w");
	fprintf (file, "%d\n", pid);
	fclose (file);
	free (buf);
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
	LOG (1, "SIGHUP received from multipath or operator");

	/*
	 * signal updatepaths() that we come from SIGHUP
	 */
	from_sighup = 1;

	/*
	 * ask for failedpaths refresh
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
                LOG(1, "Could not set SCHED_RR at priority 99");
	return;
}

int
main (int argc, char *argv[])
{
	pthread_t wait, check;
	pthread_attr_t attr;
	struct paths *failedpaths;
	pid_t pid;

	pid = sys_clone(CLONE_NEWNS, 0);
	if (pid < 0)
		exit (1);

	/*
	 * let the parent die happy
	 */
	if (pid > 0)
		exit(0);
	
	/*
	 * child's play
	 */
	openlog(argv[0], 0, LOG_DAEMON);
	LOG(1, "--------start up--------");

	pidfile(pid);
	signal_init();
	setscheduler();

	failedpaths = initpaths();

	LOG(2, "read " CONFIGFILE);
	init_data(CONFIGFILE, init_keywords);

	/*
	 * fill the voids left in the config file
	 */
	if (hwtable == NULL) {
		hwtable = vector_alloc();
		setup_default_hwtable(hwtable);
	}

	if (blist == NULL) {
		blist = vector_alloc();
		setup_default_blist(blist);
	}

	if (binvec == NULL)
		binvec = vector_alloc();

	if (prepare_namespace() < 0) {
		LOG(1, "cannot prepare namespace");
		exit_daemon(1);
	}

	mlockall(MCL_CURRENT | MCL_FUTURE);
	/*
	 * start threads
	 */
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 64 * 1024);
	
	pthread_create(&wait, &attr, waiterloop, failedpaths);
	pthread_create(&check, &attr, checkerloop, failedpaths);
	pthread_join(wait, NULL);
	pthread_join(check, NULL);

	return 0;
}
