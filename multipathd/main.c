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
#include <config.h>
#include <callout.h>
#include <util.h>
#include <blacklist.h>
#include <hwtable.h>
#include <defaults.h>
#include <structs.h>
#include <dmparser.h>
#include <devmapper.h>
#include <dict.h>
#include <discovery.h>
#include <debug.h>

#include "main.h"
#include "copy.h"
#include "clone_platform.h"
#include "pidfile.h"

#define FILE_NAME_SIZE 256
#define CMDSIZE 160

#define CALLOUT_DIR "/var/cache/multipathd"

#define LOG_MSG(a,b) \
	if (strlen(a)) { \
		log_safe(LOG_WARNING, "%s: %s", b, a); \
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
struct paths {
	pthread_mutex_t *lock;
	vector pathvec;
};

struct event_thread {
	pthread_t *thread;
	pthread_mutex_t *waiter_lock;
	int event_nr;
	char mapname[WWID_SIZE];
	struct paths *allpaths;
};

/*
 * helpers functions
 */
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
exit_daemon (int status)
{
	if (status != 0)
		fprintf(stderr, "bad exit status. see daemon.log\n");

	log_safe(LOG_INFO, "umount ramfs");
	umount(CALLOUT_DIR);

	log_safe(LOG_INFO, "unlink pidfile");
	unlink(DEFAULT_PIDFILE);

	log_safe(LOG_NOTICE, "--------shut down-------");
	log_thread_stop();
	exit(status);
}

static void *
get_devmaps (void)
{
	char *devmap;
	struct dm_task *dmt;
	struct dm_names *names = NULL;
	unsigned next = 0;
	void *nexttgt;
	vector devmaps;

	devmaps = vector_alloc();

	if (!devmaps)
		return NULL;

	if (!(dmt = dm_task_create(DM_DEVICE_LIST)))
		return NULL;

	dm_task_no_open_count(dmt);

	if (!dm_task_run(dmt)) {
		devmaps = NULL;
		goto out;
	}

	if (!(names = dm_task_get_names(dmt))) {
		devmaps = NULL;
		goto out;
	}

	if (!names->dev) {
		log_safe(LOG_WARNING, "no devmap found");
		goto out;
	}

	do {
		/*
		 * keep only multipath maps
		 */
		names = (void *) names + next;
		nexttgt = NULL;
		log_safe(LOG_DEBUG, "devmap %s", names->name);
		
		if (dm_type(names->name, "multipath")) {
			devmap = MALLOC(WWID_SIZE);

			if (!devmap)
				goto out1;

			strcpy(devmap, names->name);
			
			if (!vector_alloc_slot(devmaps)) {
				free(devmap);
				goto out1;
			}
			vector_set_slot(devmaps, devmap);
		} else
			log_safe(LOG_DEBUG,
			       "   skip non multipath target");
out1:
		next = names->next;
	} while (next);

out:
	dm_task_destroy(dmt);
	return devmaps;
}

static int
updatepaths (struct paths *allpaths, char *sysfs_path)
{
	pthread_mutex_lock(allpaths->lock);
	path_discovery(allpaths->pathvec, conf);
	pthread_mutex_unlock(allpaths->lock);

	return 0;
}

static int
mark_failed_path (struct paths *allpaths, char *mapname)
{
	struct multipath *mpp;
	struct pathgroup  *pgp;
	struct path *pp;
	struct path *app;
	char *params, *status;
	int i, j;
	int r = 1;

	if (!dm_map_present(mapname))
		return 0;

	mpp = alloc_multipath();

	if (!mpp)
		return 1;

	if (dm_get_map(mapname, &mpp->size, &params))
		return 1;

	if (dm_get_status(mapname, &status))
		return 1;
	
	pthread_mutex_lock(allpaths->lock);
	r = disassemble_map(allpaths->pathvec, params, mpp);
	pthread_mutex_unlock(allpaths->lock);
	
	if (r)
		return 1;

	disassemble_status(status, mpp);

	pthread_mutex_lock(allpaths->lock);
	vector_foreach_slot (mpp->pg, pgp, i) {
		vector_foreach_slot (pgp->paths, pp, j) {
			if (pp->dmstate != PSTATE_FAILED)
				continue;

			app = find_path_by_devt(allpaths->pathvec, pp->dev_t);
			if (app && app->state != PATH_DOWN) {
				log_safe(LOG_NOTICE, "mark %s as failed",
					pp->dev_t);
				app->state = PATH_DOWN;
			}
		}
	}
	pthread_mutex_unlock(allpaths->lock);
	free(params);
	free(status);
	free_multipath(mpp, KEEP_PATHS);

	return 0;
}

static void *
waitevent (void * et)
{
	struct event_thread *waiter;
	char buff[1];
	char cmd[CMDSIZE];
	struct dm_task *dmt;
	int event_nr;

	mlockall(MCL_CURRENT | MCL_FUTURE);
	waiter = (struct event_thread *)et;

	if (safe_snprintf(cmd, CMDSIZE, "%s %s",
			  conf->multipath, waiter->mapname)) {
		log_safe(LOG_ERR, "command too long, abord reconfigure");
		return NULL;
	}
	pthread_mutex_lock(waiter->waiter_lock);

	log_safe(LOG_DEBUG, "event number %i on %s", event_nr, waiter->mapname);

	if (!waiter->event_nr)
		waiter->event_nr = dm_geteventnr(waiter->mapname);

	if (!(dmt = dm_task_create(DM_DEVICE_WAITEVENT)))
		return 0;

	if (!dm_task_set_name(dmt, waiter->mapname))
		goto out;

	if (waiter->event_nr && !dm_task_set_event_nr(dmt, waiter->event_nr))
		goto out;

	dm_task_no_open_count(dmt);

	dm_task_run(dmt);
out:
	dm_task_destroy(dmt);

	while (1) {
		log_safe(LOG_DEBUG, "%s", cmd);
		log_safe(LOG_NOTICE, "devmap event (%i) on %s",
				waiter->event_nr, waiter->mapname);
		mark_failed_path(waiter->allpaths, waiter->mapname);
		execute_program(cmd, buff, 1);

		event_nr = dm_geteventnr(waiter->mapname);

		if (waiter->event_nr == event_nr)
			break;

		waiter->event_nr = event_nr;
	}

	/*
	 * tell waiterloop we have an event
	 */
	pthread_mutex_lock(event_lock);
	pthread_cond_signal(event);
	pthread_mutex_unlock(event_lock);
	
	/*
	 * release waiter_lock so that waiterloop knows we are gone
	 */
	pthread_mutex_unlock(waiter->waiter_lock);
	pthread_exit(waiter->thread);

	return NULL;
}

static void *
alloc_waiter (void)
{

	struct event_thread * wp;

	wp = MALLOC(sizeof(struct event_thread));

	if (!wp)
		return NULL;

	wp->thread = MALLOC(sizeof(pthread_t));

	if (!wp->thread)
		goto out;
		
	wp->waiter_lock = (pthread_mutex_t *)MALLOC(sizeof(pthread_mutex_t));

	if (!wp->waiter_lock)
		goto out1;

	pthread_mutex_init(wp->waiter_lock, NULL);
	return wp;

out1:
	free(wp->thread);
out:
	free(wp);
	return NULL;
}

static void
free_waiter (struct event_thread * wp)
{
	pthread_mutex_destroy(wp->waiter_lock);
	free(wp->thread);
	free(wp);
}

static void *
waiterloop (void *ap)
{
	struct paths *allpaths;
	vector devmaps = NULL;
	char *devmap;
	vector waiters;
	struct event_thread *wp;
	pthread_attr_t attr;
	int r;
	char buff[1];
	int i, j;

	mlockall(MCL_CURRENT | MCL_FUTURE);
	log_safe(LOG_NOTICE, "start DM events thread");

	if (sysfs_get_mnt_path(sysfs_path, FILE_NAME_SIZE)) {
		log_safe(LOG_ERR, "can not find sysfs mount point");
		return NULL;
	}

	/*
	 * inits
	 */
	allpaths = (struct paths *)ap;
	waiters = vector_alloc();

	if (!waiters)
		return;

	if (pthread_attr_init(&attr))
		return;

	pthread_attr_setstacksize(&attr, 32 * 1024);

	log_safe(LOG_NOTICE, "initial reconfigure multipath maps");
	execute_program(conf->multipath, buff, 1);

	while (1) {
		/*
		 * update devmap list
		 */
		log_safe(LOG_INFO, "refresh devmaps list");

		if (devmaps)
			strvec_free(devmaps);

		while ((devmaps = get_devmaps()) == NULL) {
			/*
			 * we're not allowed to fail here
			 */
			log_safe(LOG_ERR, "can't get devmaps ... retry");
			sleep(5);
		}

		/*
		 * update paths list
		 */
		log_safe(LOG_INFO, "refresh paths list");

		while(updatepaths(allpaths, sysfs_path)) {
			log_safe(LOG_ERR, "can't update path list ... retry");
			sleep(5);
		}

		/*
		 * start waiters on all devmaps
		 */
		log_safe(LOG_INFO, "start up event loops");

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
				wp = alloc_waiter();

				if (!wp)
					continue;

				strncpy (wp->mapname, devmap, WWID_SIZE);
				wp->allpaths = allpaths;

				if (!vector_alloc_slot(waiters)) {
					free_waiter(wp);
					continue;
				}
				vector_set_slot (waiters, wp);
			}
			
			/*
			 * event_thread struct found
			 */
			if (j < VECTOR_SIZE(waiters)) {
				r = pthread_mutex_trylock(wp->waiter_lock);

				/*
				 * thread already running : next devmap
				 */
				if (r)
					continue;
				
				pthread_mutex_unlock(wp->waiter_lock);
			}
			
			log_safe(LOG_NOTICE, "event checker startup : %s",
					wp->mapname);
			if (pthread_create(wp->thread, &attr, waitevent, wp)) {
				free_waiter(wp);
				continue;
			}
			pthread_detach(*wp->thread);
		}
		/*
		 * wait event condition
		 */
		pthread_mutex_lock (event_lock);
		pthread_cond_wait(event, event_lock);
		pthread_mutex_unlock (event_lock);
	}
	return NULL;
}

static void *
checkerloop (void *ap)
{
	struct paths *allpaths;
	struct path *pp;
	int i;
	int newstate;
	char buff[1];
	char cmd[CMDSIZE];
	char checker_msg[MAX_CHECKER_MSG_SIZE];

	mlockall(MCL_CURRENT | MCL_FUTURE);

	memset(checker_msg, 0, MAX_CHECKER_MSG_SIZE);
	allpaths = (struct paths *)ap;

	log_safe(LOG_NOTICE, "path checkers start up");

	while (1) {
		pthread_mutex_lock(allpaths->lock);
		log_safe(LOG_DEBUG, "checking paths");

		vector_foreach_slot (allpaths->pathvec, pp, i) {
			if (!pp->checkfn) {
				log_safe(LOG_ERR, "checkfn is void");
				continue;
			}
			newstate = pp->checkfn(pp->fd, checker_msg,
					       &pp->checker_context);
			
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
				if (safe_snprintf(cmd, CMDSIZE, "%s %s",
						  conf->multipath, pp->dev_t)) {
					log_safe(LOG_ERR, "command too long,"
							" abord reconfigure");
				} else {
					log_safe(LOG_DEBUG, "%s", cmd);
					log_safe(LOG_INFO,
						"reconfigure %s multipath",
						pp->dev_t);
					execute_program(cmd, buff, 1);
				}

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
		sleep(conf->checkint);
	}
	return NULL;
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
	mode_t mode = S_IRWXU;
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
	if (stat(CALLOUT_DIR, buf) < 0) {
		if (mkdir(CALLOUT_DIR, mode) < 0) {
			log_safe(LOG_ERR, "cannot create " CALLOUT_DIR);
			return -1;
		}
		log_safe(LOG_DEBUG, "created " CALLOUT_DIR);
	}

	/*
	 * compute the optimal ramdisk size
	 */
	vector_foreach_slot (conf->binvec, bin,i) {
		if ((fd = open(bin, O_RDONLY)) < 0) {
			log_safe(LOG_ERR, "cannot open %s", bin);
			return -1;
		}
		if (fstat(fd, &statbuf) < 0) {
			log_safe(LOG_ERR, "cannot stat %s", bin);
			return -1;
		}
		size += statbuf.st_size;
		close(fd);
	}
	log_safe(LOG_INFO, "ramfs maxsize is %u", (unsigned int) size);
	
	/*
	 * mount the ramfs
	 */
	if (safe_sprintf(ramfs_args, "maxsize=%u", (unsigned int) size)) {
		fprintf(stderr, "ramfs_args too small\n");
		return -1;
	}
	if (mount(NULL, CALLOUT_DIR, "ramfs", MS_SYNCHRONOUS, ramfs_args) < 0) {
		log_safe(LOG_ERR, "cannot mount ramfs on " CALLOUT_DIR);
		return -1;
	}
	log_safe(LOG_DEBUG, "mount ramfs on " CALLOUT_DIR);

	/*
	 * populate the ramfs with callout binaries
	 */
	vector_foreach_slot (conf->binvec, bin,i) {
		if (copytodir(bin, CALLOUT_DIR) < 0) {
			log_safe(LOG_ERR, "cannot copy %s in ramfs", bin);
			exit_daemon(1);
		}
		log_safe(LOG_DEBUG, "cp %s in ramfs", bin);
	}
	strvec_free(conf->binvec);

	/*
	 * bind the ramfs to :
	 * /sbin : default home of multipath ...
	 * /bin  : default home of scsi_id ...
	 * /tmp  : home of scsi_id temp files
	 */
	if (mount(CALLOUT_DIR, "/sbin", NULL, MS_BIND, NULL) < 0) {
		log_safe(LOG_ERR, "cannot bind ramfs on /sbin");
		return -1;
	}
	log_safe(LOG_DEBUG, "bind ramfs on /sbin");
	if (mount(CALLOUT_DIR, "/bin", NULL, MS_BIND, NULL) < 0) {
		log_safe(LOG_ERR, "cannot bind ramfs on /bin");
		return -1;
	}
	log_safe(LOG_DEBUG, "bind ramfs on /bin");
	if (mount(CALLOUT_DIR, "/tmp", NULL, MS_BIND, NULL) < 0) {
		log_safe(LOG_ERR, "cannot bind ramfs on /tmp");
		return -1;
	}
	log_safe(LOG_DEBUG, "bind ramfs on /tmp");

	return 0;
}
#endif

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
	log_safe(LOG_NOTICE, "SIGHUP received from multipath or operator");

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
                log_safe(LOG_WARNING, "Could not set SCHED_RR at priority 99");
	return;
}

static int
child (void * param)
{
	pthread_t wait_thr, check_thr;
	pthread_attr_t attr;
	struct paths *allpaths;

	mlockall(MCL_CURRENT | MCL_FUTURE);

	log_thread_start();
	log_safe(LOG_NOTICE, "--------start up--------");

	if (pidfile_create(DEFAULT_PIDFILE, getpid())) {
		printf("TOTO\n");
		log_thread_stop();
		exit(1);
	}
	signal_init();
	setscheduler();
	allpaths = initpaths();
	
	conf->checkint = CHECKINT;

	setlogmask(LOG_UPTO(conf->verbosity + 3));

	condlog(2, "read " DEFAULT_CONFIGFILE);
	init_data(DEFAULT_CONFIGFILE, init_keywords);

	/*
	 * fill the voids left in the config file
	 */
	if (conf->binvec == NULL) {
		conf->binvec = vector_alloc();
		push_callout("/sbin/scsi_id");
	}
	if (conf->multipath == NULL) {
		conf->multipath = MULTIPATH;
		push_callout(conf->multipath);
	}
	if (conf->hwtable == NULL) {
		conf->hwtable = vector_alloc();
		setup_default_hwtable(conf->hwtable);
	}
	if (conf->blist == NULL) {
		conf->blist = vector_alloc();
		setup_default_blist(conf->blist);
	}

#ifdef CLONE_NEWNS
	if (prepare_namespace() < 0) {
		log_safe(LOG_ERR, "cannot prepare namespace");
		exit_daemon(1);
	}
#endif

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
	extern char *optarg;
	extern int optind;
	int arg;
	int err;
	void * child_stack = (void *)malloc(CHILD_STACK_SIZE);

	if (!child_stack)
		exit(1);

	conf = alloc_config();

	if (!conf)
		exit(1);

	conf->verbosity = 2;

	while ((arg = getopt(argc, argv, ":qdlFSi:v:p:")) != EOF ) {
	switch(arg) {
		case 'v':
			if (sizeof(optarg) > sizeof(char *) ||
			    !isdigit(optarg[0]))
				exit(1);

			conf->verbosity = atoi(optarg);
			break;
		default:
			;
		}
	}

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
