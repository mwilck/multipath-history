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
#include <sysfs/libsysfs.h>

#include "hwtable.h"
#include "dict.h"
#include "parser.h"
#include "devinfo.h"
#include "checkers.h"

#define CHECKINT 5
#define MAXPATHS 2048
#define FILENAMESIZE 256
#define MAPNAMESIZE 64
#define TARGETTYPESIZE 16
#define PARAMSSIZE 2048
#define MAXMAPS 512

#define MULTIPATH "/sbin/multipath"
#define PIDFILE "/var/run/multipathd.pid"
#define CONFIGFILE "/etc/multipath.conf"

#ifndef DEBUG
#define DEBUG 1
#endif
#define LOG(x, y, z...) if (DEBUG >= x) syslog(x, y, ##z)
#define MATCH(x, y) strncmp(x, y, sizeof(y)) == 0

/* global */
int from_sighup;

struct path
{
	int major;
	int minor;
	int (*checkfn) (char *);
};

struct paths
{
	pthread_mutex_t *lock;
	struct path *paths_h;
};

struct event_thread
{
	pthread_t *thread;
	pthread_mutex_t *waiter_lock;
	char mapname[MAPNAMESIZE];
};

struct devmap
{
	char mapname[MAPNAMESIZE];
};

/* global var */
pthread_mutex_t *event_lock;
pthread_cond_t *event;

int makenode (char *devnode, int major, int minor)
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

int select_checkfn(struct path *path_p, char *devname)
{
	char vendor[8];
	char product[16];
	char rev[4];
	int i, r;
	struct hwentry * hwe;

	/* default checkfn */
	//path_p->checkfn = &readsector0;
	
	r = get_lun_strings(vendor, product, rev, devname);

	if (r) {
		LOG(2, "[select_checkfn] can not get strings");
		return r;
	}

	for (i = 0; i < VECTOR_SIZE(hwtable); i++) {
		hwe = VECTOR_SLOT(hwtable, i);
		if (MATCH(vendor, hwe->vendor) &&
		    MATCH(product, hwe->product)) {
			LOG (2, "[select_checkfn] set %s path checker for %s",
			     checker_list[hwe->checker_index].name,
			     devname);
			path_p->checkfn = checker_list[hwe->checker_index].checker;
		}
	}

	return 0;
}

int get_devmaps (struct devmap *devmaps)
{
	struct devmap *devmaps_p;
	struct dm_task *dmt, *dmt1;
	struct dm_names *names = NULL;
	unsigned next = 0;
	void *nexttgt;
	int r = 0;
	uint64_t start, length;
	char *target_type = NULL;
	char *params;

	memset (devmaps, 0, MAXMAPS * sizeof (struct devmap));

	if (!(dmt = dm_task_create(DM_DEVICE_LIST))) {
		r = 1;
		goto out;
	}

	if (!dm_task_run(dmt)) {
		r = 1;
		goto out;
	}

	if (!(names = dm_task_get_names(dmt))) {
		r = 1;
		goto out;
	}

	if (!names->dev) {
		LOG (1, "[get_devmaps] no devmap found");
		goto out;
	}

	devmaps_p = devmaps;

	do {
		/* keep only multipath maps */

		names = (void *) names + next;
		nexttgt = NULL;
		LOG (3, "[get_devmaps] iterate on devmap names : %s", names->name);

		LOG (3, "[get_devmaps]  dm_task_create(DM_DEVICE_STATUS)");
		if (!(dmt1 = dm_task_create(DM_DEVICE_STATUS)))
			goto out1;
		
		LOG (3, "[get_devmaps]  dm_task_set_name(dmt1, names->name)");
		if (!dm_task_set_name(dmt1, names->name))
			goto out1;
		
		LOG (3, "[get_devmaps]  dm_task_run(dmt1)");
		if (!dm_task_run(dmt1))
			goto out1;
		LOG (3, "[get_devmaps]  DM_DEVICE_STATUS ioctl done");
		do {
			LOG (3, "[get_devmaps]   iterate on devmap's targets");
			nexttgt = dm_get_next_target(dmt1, nexttgt,
						   &start,
						   &length,
						   &target_type,
						   &params);


			LOG (3, "[get_devmaps]   test target_type existence");
			if (!target_type)
				goto out1;
			
			LOG (3, "[get_devmaps]   test target_type is multipath");
			if (!strncmp (target_type, "multipath", 9)) {
				strcpy (devmaps_p->mapname, names->name);
				devmaps_p++;
				
				/* test vector overflow */
				if (devmaps_p - devmaps >= MAXMAPS * sizeof (struct devmap)) {
					LOG (1, "[get_devmaps] devmaps overflow");
					dm_task_destroy(dmt1);
					r = 1;
					goto out;
				}
			}

		} while (nexttgt);

out1:
		dm_task_destroy(dmt1);
		next = names->next;

	} while (next);

out:
	dm_task_destroy(dmt);

	LOG (3, "[get_devmaps] done");
	return r;
}

int checkpath (struct path *path_p)
{
	char devnode[FILENAMESIZE];
	int r;
	
	sprintf (devnode, "/tmp/.checker.%i.%i", path_p->major, path_p->minor);
	
	r = makenode (devnode, path_p->major, path_p->minor);

	if (r < 0) {
		LOG (2, "[checkpath] can not make node for %s", devnode);
		return r;
	}

	r = path_p->checkfn(devnode);
	unlink (devnode);
				
	LOG (2, "[checkpath] checked path %i:%i => %i",
	     path_p->major, path_p->minor, r);

	return r;
}

int updatepaths (struct devmap *devmaps, struct paths *failedpaths)
{
	struct path *path_p;
	struct sysfs_directory * sdir;
	struct sysfs_directory * devp;
	char sysfs_path[FILENAMESIZE];
	char path[FILENAMESIZE];
	char attr_path[FILENAMESIZE];
	char attr_buff[17];
	char *p1, *p2;
	char word[5];
	
	if (sysfs_get_mnt_path (sysfs_path, FILENAMESIZE)) {
		LOG (2, "[updatepaths] can not find sysfs mount point");
		return 1;
	}

	sprintf (path, "%s/block", sysfs_path);
	sdir = sysfs_open_directory (path);
	sysfs_read_directory (sdir);

	pthread_mutex_lock (failedpaths->lock);
	memset (failedpaths->paths_h, 0, MAXPATHS * sizeof (struct path));
	path_p = failedpaths->paths_h;

	dlist_for_each_data (sdir->subdirs, devp, struct sysfs_directory) {
		if (blacklist (devp->name)) {
			LOG (2, "[updatepaths] %s blacklisted", devp->name);
			continue;
		}

		sysfs_read_directory (devp);

		if (devp->links == NULL)
			continue;

		sprintf(attr_path, "%s/block/%s/device/generic/dev",
			sysfs_path, devp->name);

		memset (attr_buff, 0, sizeof (attr_buff));
		if (0 > sysfs_read_attribute_value(attr_path, attr_buff, 11))
			return 1;

		p1 = &word[0];
		p2 = &attr_buff[0];
		memset (&word, 0, 5 * sizeof (char));
		
		while (*p2 != ':') {
			*p1 = *p2;
			p1++;
			p2++;
		}
		path_p->major = atoi (word);

		p1 = &word[0];
		p2++;
		memset (&word, 0, 5 * sizeof (char));

		while (*p2 != ' ' && *p2 != '\0') {
			*p1 = *p2;
			p1++;
			p2++;
		}
		path_p->minor = atoi (word);

		if (!select_checkfn (path_p, devp->name) &&
		    checkpath (path_p)) {
			LOG(2, "[updatepaths] discard %i:%i as valid path",
			    path_p->major, path_p->minor);
			memset (path_p, 0, sizeof(struct path));
			continue;
		}

		LOG (2, "[updatepaths] %i:%i added to failedpaths",
		     path_p->major, path_p->minor);
		path_p++;
	}
	pthread_mutex_unlock (failedpaths->lock);
	return 0;
}

int geteventnr (char *name)
{
	struct dm_task *dmt;
	struct dm_info info;
	
	if (!(dmt = dm_task_create(DM_DEVICE_INFO)))
		return 0;

	if (!dm_task_set_name(dmt, name))
		goto out;

	if (!dm_task_run(dmt))
		goto out;

	if (!dm_task_get_info(dmt, &info))
		return 0;

	if (!info.exists) {
		LOG(1, "Device %s does not exist", name);
		return 0;
	}

out:
	dm_task_destroy(dmt);

	return info.event_nr;
}

void *waitevent (void * et)
{
	int event_nr;
	struct event_thread *waiter;

	waiter = (struct event_thread *)et;
	pthread_mutex_lock (waiter->waiter_lock);

	event_nr = geteventnr (waiter->mapname);

	struct dm_task *dmt;

	if (!(dmt = dm_task_create(DM_DEVICE_WAITEVENT)))
		return 0;

	if (!dm_task_set_name(dmt, waiter->mapname))
		goto out;

	if (event_nr && !dm_task_set_event_nr(dmt, event_nr))
		goto out;

	dm_task_run(dmt);

out:
	dm_task_destroy(dmt);

	/* tell waiterloop we have an event */
	pthread_mutex_lock (event_lock);
	pthread_cond_signal(event);
	pthread_mutex_unlock (event_lock);
	
	/* release waiter_lock so that waiterloop knows we are gone */
	pthread_mutex_unlock (waiter->waiter_lock);
	pthread_exit(waiter->thread);

	return (NULL);
}

void *waiterloop (void *ap)
{
	struct paths *failedpaths;
	struct devmap *devmaps, *devmaps_p;
	struct event_thread *waiters, *waiters_p;
	pthread_attr_t attr;
	int r;
	char *cmdargs[4] = {MULTIPATH, "-q", "-S"};
	int status;

	/* inits */
	failedpaths = (struct paths *)ap;
	devmaps = malloc (MAXMAPS * sizeof (struct devmap));
	waiters = malloc (MAXMAPS * sizeof (struct event_thread));
	memset (waiters, 0, MAXMAPS * sizeof (struct event_thread));
	pthread_attr_init (&attr);
	pthread_attr_setstacksize (&attr, 32 * 1024);

	while (1) {
		/* upon event and initial startup, do a preliminary
		   multipath exec, no signal to avoid recursion.
		   don't run multipath if we are waked from SIGHUP
		   because it already ran */
		if (!from_sighup) {
			LOG (1, "[waiterloop] exec multipath");
			if (fork () == 0)
				execve (cmdargs[0], cmdargs, NULL);
			wait (&status);
		} else
			from_sighup = 0;
		
		/* update devmap list */
		LOG (1, "[waiterloop] refresh devmaps list");
		get_devmaps (devmaps);

		/* update failed paths list */
		LOG (1, "[waiterloop] refresh failpaths list");
		updatepaths (devmaps, failedpaths);
		
		/* start waiters on all devmaps */
		LOG (1, "[waiterloop] start up event loops");
		waiters_p = waiters;
		devmaps_p = devmaps;

		while (*devmaps_p->mapname != 0x0) {
			
			/* find out if devmap already has a running waiter thread */
			while (*waiters_p->mapname != 0x0) {
				if (!strcmp (waiters_p->mapname, devmaps_p->mapname))
					break;
				waiters_p++;
			}
					
			/* no event_thread struct : init it */
			if (*waiters_p->mapname == 0x0) {
				strcpy (waiters_p->mapname, devmaps_p->mapname);
				waiters_p->thread = malloc (sizeof (pthread_t));
				waiters_p->waiter_lock = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));
				pthread_mutex_init (waiters_p->waiter_lock, NULL);
			}
			
			/* event_thread struct found */
			if (*waiters_p->mapname != 0x0) {
				r = pthread_mutex_trylock (waiters_p->waiter_lock);
				/* thread already running : out */

				if (r)
					goto out;
				
				pthread_mutex_unlock (waiters_p->waiter_lock);
			}
			
			LOG (1, "[waiterloop] create event thread for %s", waiters_p->mapname);
			pthread_create (waiters_p->thread, &attr, waitevent, waiters_p);
			pthread_detach (*waiters_p->thread);
out:
			waiters_p = waiters;
			devmaps_p++;
		}

		/* wait event condition */
		pthread_mutex_lock (event_lock);
		pthread_cond_wait(event, event_lock);
		pthread_mutex_unlock (event_lock);

		LOG (1, "[waiterloop] event caught");
	}

	return (NULL);
}

void *checkerloop (void *ap)
{
	struct paths *failedpaths;
	struct path *path_p;
	char *cmdargs[5] = {MULTIPATH, "-D", NULL, NULL, "-q"};
	char major[5];
	char minor[5];
	int status;

	failedpaths = (struct paths *)ap;

	LOG (1, "[checker thread] path checkers start up");

	while (1) {
		path_p = failedpaths->paths_h;
		pthread_mutex_lock (failedpaths->lock);
		LOG (2, "[checker thread] checking paths");
		while (path_p->major != 0) {
			
			if (checkpath (path_p)) {
				LOG (1, "[checker thread] exec multipath for device %i:%i\n",
				     path_p->major, path_p->minor);
				snprintf (major, 5, "%i", path_p->major);
				snprintf (minor, 5, "%i", path_p->minor);
				cmdargs[2] = major;
				cmdargs[3] = minor;
				if (fork () == 0)
					execve (cmdargs[0], cmdargs, NULL);

				wait (&status);
				/* MULTIPATH will send back a SIGHUP */
			}
			
			path_p++;
			
			/* test vector overflow */
			if (path_p - failedpaths->paths_h >= MAXPATHS * sizeof (struct path)) {
				LOG (1, "[checker thread] path_h overflow");
				pthread_mutex_unlock (failedpaths->lock);
				return (NULL);
			}
		}
		pthread_mutex_unlock (failedpaths->lock);
		sleep (CHECKINT);
	}

	return (NULL);
}

struct paths *initpaths (void)
{
	struct paths *failedpaths;

	failedpaths = malloc (sizeof (struct paths));
	failedpaths->paths_h = malloc (MAXPATHS * sizeof (struct path));
	failedpaths->lock = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));
	pthread_mutex_init (failedpaths->lock, NULL);
	event = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
	pthread_cond_init (event, NULL);
	event_lock = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));
	pthread_mutex_init (event_lock, NULL);
	
	return (failedpaths);
}

void pidfile (pid_t pid)
{
	FILE *file;
	struct stat *buf;

	buf = malloc (sizeof (struct stat));

	if (!stat (PIDFILE, buf)) {
		LOG(1, "[master thread] already running : out");
		free (buf);
		exit (1);
	}
		
	umask (022);
	pid = setsid ();

	if (pid < -1) {
		LOG(1, "[master thread] setsid() error");
		exit (1);
	}
	
	file = fopen (PIDFILE, "w");
	fprintf (file, "%d\n", pid);
	fclose (file);
	free (buf);
}

void *
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

void sighup (int sig)
{
	LOG (1, "[master thread] SIGHUP caught : refresh devmap list");

	/* signal updatepaths() that we come from SIGHUP */
	from_sighup = 1;

	/* ask for failedpaths refresh */
	pthread_mutex_lock (event_lock);
	pthread_cond_signal(event);
	pthread_mutex_unlock (event_lock);
}

void sigend (int sig)
{
	LOG (1, "[master thread] unlink pidfile");
	unlink (PIDFILE);
	LOG (1, "[master thread] --------shut down-------");
	exit (0);
}

void signal_init(void)
{
	signal_set(SIGHUP, sighup);
	signal_set(SIGINT, sigend);
	signal_set(SIGTERM, sigend);
	signal_set(SIGKILL, sigend);
}

int main (int argc, char *argv[])
{
	pthread_t wait, check;
	pthread_attr_t attr;
	struct paths *failedpaths;
	pid_t pid;

	pid = fork ();

	/* can't fork */
	if (pid < 0)
		exit (1);

	/* let the parent die happy */
	if (pid > 0)
		exit (0);
	
	/* child's play */
	openlog (argv[0], 0, LOG_DAEMON);
	LOG (1, "[master thread] --------start up--------");

	pidfile (pid);
	signal_init ();

	failedpaths = initpaths ();

	LOG (2, "read " CONFIGFILE);
	init_data(CONFIGFILE, init_keywords);


	pthread_attr_init (&attr);
	pthread_attr_setstacksize (&attr, 64 * 1024);
	
	pthread_create (&wait, &attr, waiterloop, failedpaths);
	pthread_create (&check, &attr, checkerloop, failedpaths);
	pthread_join (wait, NULL);
	pthread_join (check, NULL);

	return 0;
}
