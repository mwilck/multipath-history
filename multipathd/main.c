/*
 * Copyright (c) 2004, 2005 Christophe Varoqui
 * Copyright (c) 2005 Kiyoshi Ueda, NEC
 * Copyright (c) 2005 Benjamin Marzinski, Redhat
 * Copyright (c) 2005 Edward Goggin, EMC
 */
#include <unistd.h>
#include <sys/stat.h>
#include <libdevmapper.h>
#include <wait.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

/*
 * libcheckers
 */
#include <checkers.h>

/*
 * libmultipath
 */
#include <parser.h>
#include <vector.h>
#include <memory.h>
#include <config.h>
#include <util.h>
#include <hwtable.h>
#include <defaults.h>
#include <structs.h>
#include <callout.h>
#include <blacklist.h>
#include <structs_vec.h>
#include <dmparser.h>
#include <devmapper.h>
#include <sysfs.h>
#include <dict.h>
#include <discovery.h>
#include <debug.h>
#include <propsel.h>
#include <uevent.h>
#include <switchgroup.h>
#include <print.h>
#include <configure.h>

#include "main.h"
#include "pidfile.h"
#include "uxlsnr.h"
#include "uxclnt.h"
#include "cli.h"
#include "cli_handlers.h"
#include "lock.h"
#include "waiter.h"

#define FILE_NAME_SIZE 256
#define CMDSIZE 160

#define LOG_MSG(a,b) \
	if (strlen(b)) condlog(a, "%s: %s", pp->dev, b);

pthread_cond_t exit_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t exit_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * global copy of vecs for use in sig handlers
 */
struct vectors * gvecs;

static int
need_switch_pathgroup (struct multipath * mpp, int refresh)
{
	struct pathgroup * pgp;
	struct path * pp;
	unsigned int i, j;

	if (!mpp || mpp->pgfailback == -FAILBACK_MANUAL)
		return 0;

	/*
	 * Refresh path priority values
	 */
	if (refresh)
		vector_foreach_slot (mpp->pg, pgp, i)
			vector_foreach_slot (pgp->paths, pp, j)
				pathinfo(pp, conf->hwtable, DI_PRIO);

	mpp->bestpg = select_path_group(mpp);

	if (mpp->bestpg != mpp->nextpg)
		return 1;

	return 0;
}

static void
switch_pathgroup (struct multipath * mpp)
{
	mpp->stat_switchgroup++;
	dm_switchgroup(mpp->alias, mpp->bestpg);
	condlog(2, "%s: switch to path group #%i",
		 mpp->alias, mpp->bestpg);
}

static int
coalesce_maps(struct vectors *vecs, vector nmpv)
{
	struct multipath * ompp;
	vector ompv = vecs->mpvec;
	unsigned int i;
	int j;

	vector_foreach_slot (ompv, ompp, i) {
		if (!find_mp_by_wwid(nmpv, ompp->wwid)) {
			/*
			 * remove all current maps not allowed by the
			 * current configuration
			 */
			if (dm_flush_map(ompp->alias, DEFAULT_TARGET)) {
				condlog(0, "%s: unable to flush devmap",
					ompp->alias);
				/*
				 * may be just because the device is open
				 */
				if (!vector_alloc_slot(nmpv))
					return 1;

				vector_set_slot(nmpv, ompp);
				setup_multipath(vecs, ompp);

				if ((j = find_slot(ompv, (void *)ompp)) != -1)
					vector_del_slot(ompv, j);

				continue;
			}
			else {
				dm_lib_release();
				condlog(3, "%s devmap removed", ompp->alias);
			}
		}
	}
	return 0;
}

static void
sync_map_state(struct multipath *mpp)
{
	struct pathgroup *pgp;
        struct path *pp;
	unsigned int i, j;

	vector_foreach_slot (mpp->pg, pgp, i){
		vector_foreach_slot (pgp->paths, pp, j){
			if (pp->state <= PATH_UNCHECKED)
				continue;
			if ((pp->dmstate == PSTATE_FAILED ||
			     pp->dmstate == PSTATE_UNDEF) &&
			    (pp->state == PATH_UP || pp->state == PATH_GHOST))
				dm_reinstate_path(mpp->alias, pp->dev_t);
			else if ((pp->dmstate == PSTATE_ACTIVE ||
				  pp->dmstate == PSTATE_UNDEF) &&
				 (pp->state == PATH_DOWN ||
				  pp->state == PATH_SHAKY))
				dm_fail_path(mpp->alias, pp->dev_t);
		}
	}
}

static void
sync_maps_state(vector mpvec)
{
	unsigned int i;
	struct multipath *mpp;

	vector_foreach_slot (mpvec, mpp, i) 
		sync_map_state(mpp);
}

static int
flush_map(struct multipath * mpp, struct vectors * vecs)
{
	/*
	 * clear references to this map before flushing so we can ignore
	 * the spurious uevent we may generate with the dm_flush_map call below
	 */
	if (dm_flush_map(mpp->alias, DEFAULT_TARGET)) {
		/*
		 * May not really be an error -- if the map was already flushed
		 * from the device mapper by dmsetup(8) for instance.
		 */
		condlog(0, "%s: can't flush", mpp->alias);
		return 1;
	}
	else {
		dm_lib_release();
		condlog(3, "%s: devmap removed", mpp->alias);
	}

	orphan_paths(vecs->pathvec, mpp);
	remove_map(mpp, vecs, stop_waiter_thread, 1);

	return 0;
}

static int
uev_add_map (struct sysfs_device * dev, struct vectors * vecs)
{
	condlog(2, "%s: add map (uevent)", dev->kernel);
	return ev_add_map(dev, vecs);
}

int
ev_add_map (struct sysfs_device * dev, struct vectors * vecs)
{
	char * alias;
	char *dev_t;
	int major, minor;
	char * refwwid;
	struct multipath * mpp;
	int map_present;
	int r = 1;

	dev_t = sysfs_attr_get_value(dev->devpath, "dev");

	if (!dev_t || sscanf(dev_t, "%d:%d", &major, &minor) != 2)
		return 1;

	alias = dm_mapname(major, minor);
		
	if (!alias)
		return 1;
	
	map_present = dm_map_present(alias);

	if (map_present && dm_type(alias, DEFAULT_TARGET) <= 0) {
		condlog(4, "%s: not a multipath map", alias);
		return 0;
	}

	mpp = find_mp_by_alias(vecs->mpvec, alias);

	if (mpp) {
                /*
		 * Not really an error -- we generate our own uevent
		 * if we create a multipath mapped device as a result
		 * of uev_add_path
		 */
		condlog(0, "%s: devmap already registered",
			dev->kernel);
		return 0;
	}

	/*
	 * now we can register the map
	 */
	if (map_present && (mpp = add_map_without_path(vecs, minor, alias,
					start_waiter_thread))) {
		sync_map_state(mpp);
		condlog(3, "%s: devmap %s added", alias, dev->kernel);
		return 0;
	}
	refwwid = get_refwwid(dev->kernel, DEV_DEVMAP, vecs->pathvec);

	if (refwwid) {
		r = coalesce_paths(vecs, NULL, refwwid);
		dm_lib_release();
	}
	
	if (!r)
		condlog(3, "%s: devmap %s added", alias, dev->kernel);
	else
		condlog(0, "%s: uev_add_map %s failed", alias, dev->kernel);

	FREE(refwwid);
	return r;
}

static int
uev_remove_map (struct sysfs_device * dev, struct vectors * vecs)
{
	condlog(2, "%s: remove map (uevent)", dev->kernel);
	return ev_remove_map(dev->kernel, vecs);
}

int
ev_remove_map (char * devname, struct vectors * vecs)
{
	struct multipath * mpp;

	mpp = find_mp_by_str(vecs->mpvec, devname);

	if (!mpp) {
		condlog(3, "%s: devmap not registered, can't remove",
			devname);
		return 0;
	}
	flush_map(mpp, vecs);

	return 0;
}

static int
uev_umount_map (struct sysfs_device * dev, struct vectors * vecs)
{
	struct multipath * mpp;

	condlog(2, "%s: umount map (uevent)", dev->kernel);

	mpp = find_mp_by_str(vecs->mpvec, dev->kernel);

	if (!mpp)
		return 0;

	update_mpp_paths(mpp, vecs->pathvec);
	verify_paths(mpp, vecs, NULL);

	if (!VECTOR_SIZE(mpp->paths))
		flush_map(mpp, vecs);

	return 0;
}
	
static int
uev_add_path (struct sysfs_device * dev, struct vectors * vecs)
{
	condlog(2, "%s: add path (uevent)", dev->kernel);
	return (ev_add_path(dev->kernel, vecs) != 1)? 0 : 1;
}


/*
 * returns:
 * 0: added
 * 1: error
 * 2: blacklisted
 */
int
ev_add_path (char * devname, struct vectors * vecs)
{
	struct multipath * mpp;
	struct path * pp;
	char empty_buff[WWID_SIZE] = {0};

	pp = find_path_by_dev(vecs->pathvec, devname);

	if (pp) {
		condlog(0, "%s: spurious uevent, path already in pathvec",
			devname);
		if (pp->mpp)
			return 0;
	}
	else {
		/*
		 * get path vital state
		 */
		if (!(pp = store_pathinfo(vecs->pathvec, conf->hwtable,
		      devname, DI_ALL))) {
			condlog(0, "%s: failed to store path info", devname);
			return 1;
		}
		pp->checkint = conf->checkint;
	}

	/*
	 * need path UID to go any further
	 */
	if (memcmp(empty_buff, pp->wwid, WWID_SIZE) == 0) {
		condlog(0, "%s: failed to get path uid", devname);
		return 1; /* leave path added to pathvec */
	}
	if (filter_path(conf, pp)){
		int i = find_slot(vecs->pathvec, (void *)pp);
		if (i != -1)
			vector_del_slot(vecs->pathvec, i);
		free_path(pp);
		return 2;
	}	
	mpp = pp->mpp = find_mp_by_wwid(vecs->mpvec, pp->wwid);
rescan:
	if (mpp) {
		if (adopt_paths(vecs->pathvec, mpp))
			return 1; /* leave path added to pathvec */

		verify_paths(mpp, vecs, NULL);
		mpp->action = ACT_RELOAD;
	}
	else {
		if ((mpp = add_map_with_path(vecs, pp, 1)))
			mpp->action = ACT_CREATE;
		else
			return 1; /* leave path added to pathvec */
	}

	/*
	 * push the map to the device-mapper
	 */
	if (setup_map(mpp)) {
		condlog(0, "%s: failed to setup map for addition of new "
			"path %s", mpp->alias, devname);
		goto out;
	}
	/*
	 * reload the map for the multipath mapped device
	 */
	if (domap(mpp) <= 0) {
		condlog(0, "%s: failed in domap for addition of new "
			"path %s", mpp->alias, devname);
		/*
 		 * deal with asynchronous uevents :((
 		 */
		if (mpp->action == ACT_RELOAD) {
			condlog(0, "%s: uev_add_path sleep", mpp->alias);
			sleep(1);
			update_mpp_paths(mpp, vecs->pathvec);
			goto rescan;
		}
		else
			goto out;
	}
	dm_lib_release();

	/*
	 * update our state from kernel regardless of create or reload
	 */
	if (setup_multipath(vecs, mpp))
		goto out;

	sync_map_state(mpp);

	if (mpp->action == ACT_CREATE &&
	    start_waiter_thread(mpp, vecs))
			goto out;

	condlog(3, "%s path added to devmap %s", devname, mpp->alias);
	return 0;

out:
	remove_map(mpp, vecs, NULL, 1);
	return 1;
}

static int
uev_remove_path (struct sysfs_device * dev, struct vectors * vecs)
{
	condlog(2, "%s: remove path (uevent)", dev->kernel);
	return ev_remove_path(dev->kernel, vecs);
}

int
ev_remove_path (char * devname, struct vectors * vecs)
{
	struct multipath * mpp;
	struct path * pp;
	int i;
	int rm_path = 1;

	pp = find_path_by_dev(vecs->pathvec, devname);

	if (!pp) {
		condlog(0, "%s: spurious uevent, path not in pathvec", devname);
		return 1;
	}

	/*
	 * avoid referring to the map of an orphanned path
	 */
	if ((mpp = pp->mpp)) {

		/*
		 * remove the map IFF removing the last path
		 */
		if (pathcount(mpp, PATH_WILD) > 1) {
			vector rpvec = vector_alloc();

			/*
	 	 	 * transform the mp->pg vector of vectors of paths
	 	 	 * into a mp->params string to feed the device-mapper
	 	 	 */
			update_mpp_paths(mpp, vecs->pathvec);
			if ((i = find_slot(mpp->paths, (void *)pp)) != -1)
				vector_del_slot(mpp->paths, i);

			if (VECTOR_SIZE(mpp->paths) == 0) {
				char alias[WWID_SIZE];

				/*
				 * flush_map will fail if the device is open
				 */
				strncpy(alias, mpp->alias, WWID_SIZE);
				if (flush_map(mpp, vecs))
					rm_path = 0;
				else
					condlog(3, "%s: removed map after removing"
						" multiple paths", alias);
			}
			else {
				if (setup_map(mpp)) {
					condlog(0, "%s: failed to setup map for"
						" removal of path %s", mpp->alias, devname);
					free_pathvec(rpvec, KEEP_PATHS);
					goto out;
				}
				/*
	 	 	 	 * reload the map
	 	 	 	 */
				mpp->action = ACT_RELOAD;
				if (domap(mpp) <= 0) {
					condlog(0, "%s: failed in domap for "
						"removal of path %s",
						mpp->alias, devname);
					/*
					 * Delete path from pathvec so that
					 * update_mpp_paths wont find it later
					 * when/if another path is removed.
					 */
					if ((i = find_slot(vecs->pathvec, (void *)pp)) != -1)
						vector_del_slot(vecs->pathvec, i);
					free_path(pp);
					return 1;
				}
				/*
				 * update our state from kernel
				 */
				if (setup_multipath(vecs, mpp)) {
					free_pathvec(rpvec, KEEP_PATHS);
					goto out;
				}
				sync_map_state(mpp);

				condlog(3, "%s: path removed from map %s",
					devname, mpp->alias);
			}
			free_pathvec(rpvec, KEEP_PATHS);
		}
		else {
			char alias[WWID_SIZE];

			/*
			 * flush_map will fail if the device is open
			 */
			strncpy(alias, mpp->alias, WWID_SIZE);
			if (flush_map(mpp, vecs))
				rm_path = 0;
			else
				condlog(3, "%s: removed map", alias);
		}
	}

	if (rm_path) {
		if ((i = find_slot(vecs->pathvec, (void *)pp)) != -1)
			vector_del_slot(vecs->pathvec, i);
		free_path(pp);
	}

	return 0;

out:
	remove_map(mpp, vecs, stop_waiter_thread, 1);
	return 1;
}

static int
map_discovery (struct vectors * vecs)
{
	struct multipath * mpp;
	unsigned int i;

	if (dm_get_maps(vecs->mpvec, "multipath"))
		return 1;

	vector_foreach_slot (vecs->mpvec, mpp, i)
		if (setup_multipath(vecs, mpp))
			return 1;

	return 0;
}

int
uxsock_trigger (char * str, char ** reply, int * len, void * trigger_data)
{
	struct vectors * vecs;
	int r;
	
	*reply = NULL;
	*len = 0;
	vecs = (struct vectors *)trigger_data;

	pthread_cleanup_push(cleanup_lock, vecs->lock);
	lock(vecs->lock);

	r = parse_cmd(str, reply, len, vecs);

	if (r > 0) {
		*reply = STRDUP("fail\n");
		*len = strlen(*reply) + 1;
		r = 1;
	}
	else if (!r && *len == 0) {
		*reply = STRDUP("ok\n");
		*len = strlen(*reply) + 1;
		r = 0;
	}
	/* else if (r < 0) leave *reply alone */

	lock_cleanup_pop(vecs->lock);
	return r;
}

static int
uev_discard(char * devpath)
{
	char a[10], b[10];

	/*
	 * keep only block devices, discard partitions
	 */
	if (sscanf(devpath, "/block/%10s", a) != 1 ||
	    sscanf(devpath, "/block/%10[^/]/%10s", a, b) == 2) {
		condlog(4, "discard event on %s", devpath);
		return 1;
	}
	return 0;
}

int 
uev_trigger (struct uevent * uev, void * trigger_data)
{
	int r = 0;
	struct sysfs_device *sysdev;
	struct vectors * vecs;

	vecs = (struct vectors *)trigger_data;

	if (uev_discard(uev->devpath))
		return 0;

	sysdev = sysfs_device_get(uev->devpath);
	lock(vecs->lock);

	/*
	 * device map event
	 * Add events are ignored here as the tables
	 * are not fully initialised then.
	 */
	if (!strncmp(sysdev->kernel, "dm-", 3)) {
		if (!strncmp(uev->action, "change", 6)) {
			r = uev_add_map(sysdev, vecs);
			goto out;
		}
		if (!strncmp(uev->action, "remove", 6)) {
			r = uev_remove_map(sysdev, vecs);
			goto out;
		}
		if (!strncmp(uev->action, "umount", 6)) {
			r = uev_umount_map(sysdev, vecs);
			goto out;
		}
		goto out;
	}
	
	/*
	 * path add/remove event
	 */
	if (filter_devnode(conf->blist_devnode, conf->elist_devnode,
		 	   sysdev->kernel) > 0)
		goto out;

	if (!strncmp(uev->action, "add", 3)) {
		r = uev_add_path(sysdev, vecs);
		goto out;
	}
	if (!strncmp(uev->action, "remove", 6)) {
		r = uev_remove_path(sysdev, vecs);
		goto out;
	}

out:
	unlock(vecs->lock);
	return r;
}

static void *
ueventloop (void * ap)
{
	if (uevent_listen(&uev_trigger, ap))
		fprintf(stderr, "error starting uevent listener");
		
	return NULL;
}

static void *
uxlsnrloop (void * ap)
{
	if (cli_init())
		return NULL;

	set_handler_callback(LIST+PATHS, cli_list_paths);
	set_handler_callback(LIST+MAPS, cli_list_maps);
	set_handler_callback(LIST+MAPS+STATUS, cli_list_maps_status);
	set_handler_callback(LIST+MAPS+STATS, cli_list_maps_stats);
	set_handler_callback(LIST+MAPS+TOPOLOGY, cli_list_maps_topology);
	set_handler_callback(LIST+TOPOLOGY, cli_list_maps_topology);
	set_handler_callback(LIST+MAP+TOPOLOGY, cli_list_map_topology);
	set_handler_callback(LIST+CONFIG, cli_list_config);
	set_handler_callback(LIST+BLACKLIST, cli_list_blacklist);
	set_handler_callback(LIST+DEVICES, cli_list_devices);
	set_handler_callback(ADD+PATH, cli_add_path);
	set_handler_callback(DEL+PATH, cli_del_path);
	set_handler_callback(ADD+MAP, cli_add_map);
	set_handler_callback(DEL+MAP, cli_del_map);
	set_handler_callback(SWITCH+MAP+GROUP, cli_switch_group);
	set_handler_callback(RECONFIGURE, cli_reconfigure);
	set_handler_callback(SUSPEND+MAP, cli_suspend);
	set_handler_callback(RESUME+MAP, cli_resume);
	set_handler_callback(REINSTATE+PATH, cli_reinstate);
	set_handler_callback(FAIL+PATH, cli_fail);

	uxsock_listen(&uxsock_trigger, ap);

	return NULL;
}

static int
exit_daemon (int status)
{
	if (status != 0)
		fprintf(stderr, "bad exit status. see daemon.log\n");

	condlog(3, "unlink pidfile");
	unlink(DEFAULT_PIDFILE);

	lock(&exit_mutex);
	pthread_cond_signal(&exit_cond);
	unlock(&exit_mutex);

	return status;
}

static void
fail_path (struct path * pp, int del_active)
{
	if (!pp->mpp)
		return;

	condlog(2, "checker failed path %s in map %s",
		 pp->dev_t, pp->mpp->alias);

	dm_fail_path(pp->mpp->alias, pp->dev_t);
	if (del_active)
		update_queue_mode_del_path(pp->mpp);
}

/*
 * caller must have locked the path list before calling that function
 */
static void
reinstate_path (struct path * pp, int add_active)
{
	if (!pp->mpp)
		return;

	if (dm_reinstate_path(pp->mpp->alias, pp->dev_t))
		condlog(0, "%s: reinstate failed", pp->dev_t);
	else {
		condlog(2, "%s: reinstated", pp->dev_t);
		if (add_active)
			update_queue_mode_add_path(pp->mpp);
	}
}

static void
enable_group(struct path * pp)
{
	struct pathgroup * pgp;

	/*
	 * if path is added through uev_add_path, pgindex can be unset.
	 * next update_strings() will set it, upon map reload event.
	 *
	 * we can safely return here, because upon map reload, all
	 * PG will be enabled.
 	 */
	if (!pp->mpp->pg || !pp->pgindex)
		return;

	pgp = VECTOR_SLOT(pp->mpp->pg, pp->pgindex - 1);
	
	if (pgp->status == PGSTATE_DISABLED) {
		condlog(2, "%s: enable group #%i", pp->mpp->alias, pp->pgindex);
		dm_enablegroup(pp->mpp->alias, pp->pgindex);
	}
}

static void
mpvec_garbage_collector (struct vectors * vecs)
{
	struct multipath * mpp;
	unsigned int i;

	vector_foreach_slot (vecs->mpvec, mpp, i) {
		if (mpp && mpp->alias && !dm_map_present(mpp->alias)) {
			condlog(2, "%s: remove dead map", mpp->alias);
			remove_map(mpp, vecs, stop_waiter_thread, 1);
			i--;
		}
	}
}

static void
defered_failback_tick (vector mpvec)
{
	struct multipath * mpp;
	unsigned int i;

	vector_foreach_slot (mpvec, mpp, i) {
		/*
		 * defered failback getting sooner
		 */
		if (mpp->pgfailback > 0 && mpp->failback_tick > 0) {
			mpp->failback_tick--;

			if (!mpp->failback_tick && need_switch_pathgroup(mpp, 1))
				switch_pathgroup(mpp);
		}
	}
}

static void
retry_count_tick(vector mpvec)
{
	struct multipath *mpp;
	unsigned int i;

	vector_foreach_slot (mpvec, mpp, i) {
		if (mpp->retry_tick) {
			mpp->stat_total_queueing_time++;
			condlog(4, "%s: Retrying.. No active path", mpp->alias);
			if(--mpp->retry_tick == 0) {
				dm_queue_if_no_path(mpp->alias, 0);
				condlog(2, "%s: Disable queueing", mpp->alias);
			}
		}
	}
}

static void *
checkerloop (void *ap)
{
	struct vectors *vecs;
	struct path *pp;
	int count = 0;
	int newstate;
	unsigned int i;

	mlockall(MCL_CURRENT | MCL_FUTURE);
	vecs = (struct vectors *)ap;
	condlog(2, "path checkers start up");

	/*
	 * init the path check interval
	 */
	vector_foreach_slot (vecs->pathvec, pp, i) {
		pp->checkint = conf->checkint;
	}

	while (1) {
		pthread_cleanup_push(cleanup_lock, vecs->lock);
		lock(vecs->lock);
		condlog(4, "tick");

		vector_foreach_slot (vecs->pathvec, pp, i) {
			if (!pp->mpp)
				continue;

			if (pp->tick && --pp->tick)
				continue; /* don't check this path yet */

			/*
			 * provision a next check soonest,
			 * in case we exit abnormaly from here
			 */
			pp->tick = conf->checkint;
			
			if (!checker_selected(&pp->checker)) {
				pathinfo(pp, conf->hwtable, DI_SYSFS);
				select_checker(pp);
			}

			if (!checker_selected(&pp->checker)) {
				condlog(0, "%s: checker is not set", pp->dev);
				continue;
			}
			newstate = checker_check(&pp->checker);
			
			if (newstate < 0) {
				condlog(2, "%s: unusable path", pp->dev);
				pathinfo(pp, conf->hwtable, 0);
				continue;
			}

			if (newstate != pp->state) {
				int oldstate = pp->state;
				pp->state = newstate;
				LOG_MSG(1, checker_message(&pp->checker));

				/*
				 * upon state change, reset the checkint
				 * to the shortest delay
				 */
				pp->checkint = conf->checkint;

				if (newstate == PATH_DOWN ||
				    newstate == PATH_SHAKY ||
				    update_multipath_strings(pp->mpp,
					    		     vecs->pathvec)) {
					/*
					 * proactively fail path in the DM
					 */
					if (oldstate == PATH_UP ||
					    oldstate == PATH_GHOST)
						fail_path(pp, 1);
					else
						fail_path(pp, 0);

					/*
					 * cancel scheduled failback
					 */
					pp->mpp->failback_tick = 0;

					pp->mpp->stat_path_failures++;
					continue;
				}

				/*
				 * reinstate this path
				 */
				if (oldstate != PATH_UP &&
				    oldstate != PATH_GHOST)
					reinstate_path(pp, 1);
				else
					reinstate_path(pp, 0);

				/*
				 * schedule [defered] failback
				 */
				if (pp->mpp->pgfailback > 0)
					pp->mpp->failback_tick =
						pp->mpp->pgfailback + 1;
				else if (pp->mpp->pgfailback == -FAILBACK_IMMEDIATE &&
				    need_switch_pathgroup(pp->mpp, 1))
					switch_pathgroup(pp->mpp);

				/*
				 * if at least one path is up in a group, and
				 * the group is disabled, re-enable it
				 */
				if (newstate == PATH_UP)
					enable_group(pp);
			}
			else if (newstate == PATH_UP || newstate == PATH_GHOST) {
				LOG_MSG(4, checker_message(&pp->checker));
				/*
				 * double the next check delay.
				 * max at conf->max_checkint
				 */
				if (pp->checkint < (conf->max_checkint / 2))
					pp->checkint = 2 * pp->checkint;
				else
					pp->checkint = conf->max_checkint;

				pp->tick = pp->checkint;
				condlog(4, "%s: delay next check %is",
						pp->dev_t, pp->tick);
			}
			else if (newstate == PATH_DOWN)
				LOG_MSG(2, checker_message(&pp->checker));

			pp->state = newstate;

			/*
			 * path prio refreshing
			 */
			condlog(4, "path prio refresh");
			pathinfo(pp, conf->hwtable, DI_PRIO);

			if (need_switch_pathgroup(pp->mpp, 0)) {
				if (pp->mpp->pgfailback > 0 &&
				    pp->mpp->failback_tick <= 0)
					pp->mpp->failback_tick =
						pp->mpp->pgfailback + 1;
				else if (pp->mpp->pgfailback ==
						-FAILBACK_IMMEDIATE)
					switch_pathgroup(pp->mpp);
			}
		}
		defered_failback_tick(vecs->mpvec);
		retry_count_tick(vecs->mpvec);

		if (count)
			count--;
		else {
			condlog(4, "map garbage collection");
			mpvec_garbage_collector(vecs);
			count = MAPGCINT;
		}
		
		lock_cleanup_pop(vecs->lock);
		sleep(1);
	}
	return NULL;
}

int
configure (struct vectors * vecs, int start_waiters)
{
	struct multipath * mpp;
	struct path * pp;
	vector mpvec;
	int i;

	if (!vecs->pathvec && !(vecs->pathvec = vector_alloc()))
		return 1;
	
	if (!vecs->mpvec && !(vecs->mpvec = vector_alloc()))
		return 1;
	
	if (!(mpvec = vector_alloc()))
		return 1;

	/*
	 * probe for current path (from sysfs) and map (from dm) sets
	 */
	path_discovery(vecs->pathvec, conf, DI_ALL);

	vector_foreach_slot (vecs->pathvec, pp, i){
		if (filter_path(conf, pp)){
			vector_del_slot(vecs->pathvec, i);
			free_path(pp);
			i--;
		}	
		else
			pp->checkint = conf->checkint;
	}
	if (map_discovery(vecs))
		return 1;

	/*
	 * create new set of maps & push changed ones into dm
	 */
	if (coalesce_paths(vecs, mpvec, NULL))
		return 1;

	/*
	 * may need to remove some maps which are no longer relevant
	 * e.g., due to blacklist changes in conf file
	 */
	if (coalesce_maps(vecs, mpvec))
		return 1;

	dm_lib_release();

	sync_maps_state(mpvec);

	/*
	 * purge dm of old maps
	 */
	remove_maps(vecs, NULL);

	/*
	 * save new set of maps formed by considering current path state
	 */
	vector_free(vecs->mpvec);
	vecs->mpvec = mpvec;

	/*
	 * start dm event waiter threads for these new maps
	 */
	vector_foreach_slot(vecs->mpvec, mpp, i) {
		if (setup_multipath(vecs, mpp))
			return 1;
		if (start_waiters)
			if (start_waiter_thread(mpp, vecs))
				return 1;
	}
	return 0;
}

int
reconfigure (struct vectors * vecs)
{
	struct config * old = conf;

	/*
	 * free old map and path vectors ... they use old conf state
	 */
	if (VECTOR_SIZE(vecs->mpvec))
		remove_maps(vecs, stop_waiter_thread);

	if (VECTOR_SIZE(vecs->pathvec))
		free_pathvec(vecs->pathvec, FREE_PATHS);

	vecs->pathvec = NULL;
	conf = NULL;

	if (load_config(DEFAULT_CONFIGFILE))
		return 1;

	conf->verbosity = old->verbosity;

	if (!conf->checkint) {
		conf->checkint = DEFAULT_CHECKINT;
		conf->max_checkint = MAX_CHECKINT(conf->checkint);
	}
	configure(vecs, 1);
	free_config(old);
	return 0;
}

static struct vectors *
init_vecs (void)
{
	struct vectors * vecs;

	vecs = (struct vectors *)MALLOC(sizeof(struct vectors));

	if (!vecs)
		return NULL;

	vecs->lock = 
		(pthread_mutex_t *)MALLOC(sizeof(pthread_mutex_t));

	if (!vecs->lock)
		goto out;

	pthread_mutex_init(vecs->lock, NULL);

	return vecs;

out:
	FREE(vecs);
	condlog(0, "failed to init paths");
	return NULL;
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
	condlog(2, "reconfigure (SIGHUP)");

	lock(gvecs->lock);
	reconfigure(gvecs);
	unlock(gvecs->lock);

#ifdef _DEBUG_
	dbg_free_final(NULL);
#endif
}

static void
sigend (int sig)
{
	exit_daemon(0);
}

static void
sigusr1 (int sig)
{
	condlog(3, "SIGUSR1 received");
}

static void
signal_init(void)
{
	signal_set(SIGHUP, sighup);
	signal_set(SIGUSR1, sigusr1);
	signal_set(SIGINT, sigend);
	signal_set(SIGTERM, sigend);
	signal(SIGPIPE, SIG_IGN);
}

static void
setscheduler (void)
{
        int res;
	static struct sched_param sched_param = {
		.sched_priority = 99
	};

        res = sched_setscheduler (0, SCHED_RR, &sched_param);

        if (res == -1)
                condlog(LOG_WARNING, "Could not set SCHED_RR at priority 99");
	return;
}

static void
set_oom_adj (int val)
{
	FILE *fp;

	fp = fopen("/proc/self/oom_adj", "w");

	if (!fp)
		return;

	fprintf(fp, "%i", val);
	fclose(fp);
}
	
static int
child (void * param)
{
	pthread_t check_thr, uevent_thr, uxlsnr_thr;
	pthread_attr_t attr;
	struct vectors * vecs;

	mlockall(MCL_CURRENT | MCL_FUTURE);

	if (logsink)
		log_thread_start();

	condlog(2, "--------start up--------");
	condlog(2, "read " DEFAULT_CONFIGFILE);

	if (load_config(DEFAULT_CONFIGFILE))
		exit(1);

	setlogmask(LOG_UPTO(conf->verbosity + 3));

	/*
	 * fill the voids left in the config file
	 */
	if (!conf->checkint) {
		conf->checkint = DEFAULT_CHECKINT;
		conf->max_checkint = MAX_CHECKINT(conf->checkint);
	}

	if (pidfile_create(DEFAULT_PIDFILE, getpid())) {
		if (logsink)
			log_thread_stop();

		exit(1);
	}
	signal_init();
	setscheduler();
	set_oom_adj(-16);
	vecs = gvecs = init_vecs();

	if (!vecs)
		exit(1);

	if (sysfs_init(conf->sysfs_dir, FILE_NAME_SIZE)) {
		condlog(0, "can not find sysfs mount point");
		exit(1);
	}

	/*
	 * fetch and configure both paths and multipaths
	 */
	if (configure(vecs, 1)) {
		condlog(0, "failure during configuration");
		exit(1);
	}

	/*
	 * start threads
	 */
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 64 * 1024);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	
	pthread_create(&check_thr, &attr, checkerloop, vecs);
	pthread_create(&uevent_thr, &attr, ueventloop, vecs);
	pthread_create(&uxlsnr_thr, &attr, uxlsnrloop, vecs);

	pthread_cond_wait(&exit_cond, &exit_mutex);

	/*
	 * exit path
	 */
	lock(vecs->lock);
	remove_maps(vecs, stop_waiter_thread);
	free_pathvec(vecs->pathvec, FREE_PATHS);

	pthread_cancel(check_thr);
	pthread_cancel(uevent_thr);
	pthread_cancel(uxlsnr_thr);

	sysfs_cleanup();

	free_keys(keys);
	keys = NULL;
	free_handlers(handlers);
	handlers = NULL;
	free_polls();

	unlock(vecs->lock);
	pthread_mutex_destroy(vecs->lock);
	FREE(vecs->lock);
	vecs->lock = NULL;
	FREE(vecs);
	vecs = NULL;
	free_config(conf);
	conf = NULL;

	condlog(2, "--------shut down-------");
	
	if (logsink)
		log_thread_stop();

	dm_lib_release();
	dm_lib_exit();

#ifdef _DEBUG_
	dbg_free_final(NULL);
#endif

	exit(0);
}

static int
daemonize(void)
{
	int pid;
	int in_fd, out_fd;

	if( (pid = fork()) < 0){
		fprintf(stderr, "Failed first fork : %s\n", strerror(errno));
		return -1;
	}
	else if (pid != 0)
		return pid;

	setsid();

	if ( (pid = fork()) < 0)
		fprintf(stderr, "Failed second fork : %s\n", strerror(errno));
	else if (pid != 0)
		_exit(0);

	in_fd = open("/dev/null", O_RDONLY);
	if (in_fd < 0){
		fprintf(stderr, "cannot open /dev/null for input : %s\n",
			strerror(errno));
		_exit(0);
	}
	out_fd = open("/dev/console", O_WRONLY);
	if (out_fd < 0){
		fprintf(stderr, "cannot open /dev/console for output : %s\n",
			strerror(errno));
		_exit(0);
	}

	close(STDIN_FILENO);
	dup(in_fd);
	close(STDOUT_FILENO);
	dup(out_fd);
	close(STDERR_FILENO);
	dup(out_fd);

	close(in_fd);
	close(out_fd);
	chdir("/");
	umask(0);
	return 0;
}

int
main (int argc, char *argv[])
{
	extern char *optarg;
	extern int optind;
	int arg;
	int err;
	
	logsink = 1;
	dm_init();

	if (getuid() != 0) {
		fprintf(stderr, "need to be root\n");
		exit(1);
	}

	/* make sure we don't lock any path */
	chdir("/");
	umask(umask(077) | 022);

	conf = alloc_config();

	if (!conf)
		exit(1);

	while ((arg = getopt(argc, argv, ":dv:k::")) != EOF ) {
	switch(arg) {
		case 'd':
			logsink = 0;
			//debug=1; /* ### comment me out ### */
			break;
		case 'v':
			if (sizeof(optarg) > sizeof(char *) ||
			    !isdigit(optarg[0]))
				exit(1);

			conf->verbosity = atoi(optarg);
			break;
		case 'k':
			uxclnt(optarg);
			exit(0);
		default:
			;
		}
	}

	if (!logsink)
		err = 0;
	else
		err = daemonize();
	
	if (err < 0)
		/* error */
		exit(1);
	else if (err > 0)
		/* parent dies */
		exit(0);
	else
		/* child lives */
		return (child(NULL));
}

