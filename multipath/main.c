/*
 * Soft:        multipath device mapper target autoconfig
 *
 * Version:     $Id: main.h,v 0.0.1 2003/09/18 15:13:38 cvaroqui Exp $
 *
 * Author:      Copyright (C) 2003 Christophe Varoqui
 *
 *              This program is distributed in the hope that it will be useful,
 *              but WITHOUT ANY WARRANTY; without even the implied warranty of
 *              MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *              See the GNU General Public License for more details.
 *
 *              This program is free software; you can redistribute it and/or
 *              modify it under the terms of the GNU General Public License
 *              as published by the Free Software Foundation; either version
 *              2 of the License, or (at your option) any later version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <signal.h>
#include <sys/ioctl.h>

#include <parser.h>
#include <vector.h>
#include <memory.h>
#include <sysfs/dlist.h>
#include <sysfs/libsysfs.h>
#include <libdevmapper.h>
#include <checkers.h>
#include <path_state.h>

#include "main.h"
#include "devinfo.h"
#include "config.h"
#include "pgpolicies.h"
#include "dict.h"
#include "callout.h"
#include "debug.h"

/* helpers */
#define argis(x) if (0 == strcmp (x, argv[i]))
#define MATCH(x,y) 0 == strncmp (x, y, strlen(y))

static int
devt2devname (char *devname, char *devt)
{
	struct sysfs_directory * sdir;
	struct sysfs_directory * devp;
	char sysfs_path[FILE_NAME_SIZE];
	char block_path[FILE_NAME_SIZE];
	char attr_path[FILE_NAME_SIZE];
	char attr_value[16];

	if (sysfs_get_mnt_path(sysfs_path, FILE_NAME_SIZE)) {
		fprintf(stderr, "-D feature available with sysfs only\n");
		exit(1);
	}
		
	if(safe_sprintf(block_path, "%s/block", sysfs_path)) {
		fprintf(stderr, "block_path too small\n");
		exit(1);
	}
	sdir = sysfs_open_directory(block_path);
	sysfs_read_directory(sdir);

	dlist_for_each_data (sdir->subdirs, devp, struct sysfs_directory) {
		if(safe_sprintf(attr_path, "%s/%s/dev",
				block_path, devp->name)) {
			fprintf(stderr, "attr_path too small\n");
			exit(1);
		}
		sysfs_read_attribute_value(attr_path, attr_value,
					   sizeof(attr_value));

		if (!strncmp(attr_value, devt, strlen(devt))) {
			if(safe_sprintf(attr_path, "%s/%s",
					block_path, devp->name)) {
				fprintf(stderr, "attr_path too small\n");
				exit(1);
			}
			sysfs_get_name_from_path(attr_path, devname,
						 FILE_NAME_SIZE);
			break;
		}
	}
	sysfs_close_directory(sdir);
	return 0;
}

static int
filepresent (char * run) {
	struct stat buf;

	if(!stat(run, &buf))
		return 1;
	return 0;
}

static int
blacklist (char * dev) {
	int i;
	char *p;

	vector_foreach_slot (conf->blist, p, i)
		if (memcmp(dev, p, strlen(p)) == 0)
			return 1;
	
	return 0;
}

static int
select_checkfn(struct path *pp)
{
	char checker_name[CHECKER_NAME_SIZE];
	int i;
	struct hwentry * hwe;

	/*
	 * default checkfn
	 */
	pp->checkfn = &readsector0;

	vector_foreach_slot (conf->hwtable, hwe, i) {
		if (MATCH(hwe->vendor, pp->vendor_id) &&
		    MATCH(hwe->product, pp->product_id) &&
		    hwe->checker_index > 0) {
			get_checker_name(checker_name, hwe->checker_index);
			dbg("set %s path checker for %s",
				checker_name, pp->dev);
			pp->checkfn = get_checker_addr(hwe->checker_index);
			return 0;
		}
	}
	get_checker_name(checker_name, READSECTOR0);
	dbg("set %s path checker for %s", checker_name, pp->dev);
	return 0;
}

static int
devinfo (struct path *curpath)
{
	int i;
	struct hwentry * hwe;
	char buff[100];
	char prio[16];

	/*
	 * fetch info available in sysfs
	 */
	if (sysfs_devinfo(curpath))
		return 1;

	/*
	 * then those not available through sysfs
	 */
	get_serial(curpath->serial, curpath->dev_t);
	curpath->claimed = get_claimed(curpath->dev_t);

	/*
	 * get path state, no message collection, no context
	 */
	select_checkfn(curpath);
	curpath->state = checkpath(curpath->dev_t, curpath->checkfn,
				   NULL, NULL);
	dbg("path %s state : %i", curpath->dev, curpath->state);
	
	/*
	 * get path prio
	 */
	if(safe_sprintf(buff, "%s /block/%s",
			conf->default_getprio, curpath->dev)) {
		fprintf(stderr, "buff too small\n");
		exit(1);
	}

	dbg("get prio callout :");
	dbg("==================");

	if (execute_program(buff, prio, 16) == 0)
		curpath->priority = atoi(prio);
	else {
		dbg("error calling out %s", buff);
		curpath->priority = 1;
	}
	dbg("devinfo found prio : %u", curpath->priority);

	/*
	 * get path uid
	 */
	vector_foreach_slot (conf->hwtable, hwe, i) {
		if (MATCH (curpath->vendor_id, hwe->vendor) &&
		    MATCH (curpath->product_id, hwe->product)) {
			/*
			 * callout program
			 */
			dbg("get uid callout :");
			dbg("=================");
			if(safe_sprintf(buff, "%s /block/%s",
				 hwe->getuid, curpath->dev)) {
				fprintf(stderr, "buff too small\n");
				exit(1);
			}
			if (execute_program(buff, curpath->wwid,
			    WWID_SIZE) == 0) {
				dbg("devinfo found uid : %s", curpath->wwid);
				return 0;
			}
			dbg("error calling out %s", buff);
			dbg("falling back to internal getuid function");

			/*
			 * fallback
			 */
			if (!get_evpd_wwid(curpath->dev_t, curpath->wwid)) {
				dbg("devinfo found uid : %s", curpath->wwid);
				return 0;
			}
			/*
			 * no wwid : blank for safety
			 */
			dbg("unable to fetch a wwid : set to 0x0");
			memset(curpath->wwid, 0, WWID_SIZE);
			return 1;
		}
	}
	dbg("devinfo out : no match ... apply defaults");

	/*
	 * chances are we deal directly with disks here (no FC ctlr)
	 * we need scsi_id for this fallback to work
	 *
	 * incidentaly, dealing with this case will make parallel SCSI
	 * disks treated as 1-path multipaths, which is good : wider audience !
	 */
	if(safe_sprintf(buff, "%s /block/%s",
			conf->default_getuid, curpath->dev)) {
		fprintf(stderr, "buff too small\n");
		exit(1);
	}

	dbg("default get uid callout :");
	dbg("=========================");

	if (execute_program(buff, curpath->wwid, WWID_SIZE) == 0) {
		dbg("devinfo found uid : %s", curpath->wwid);
		return 0;
	}
	dbg("error calling out %s", buff);
	memset(curpath->wwid, 0, WWID_SIZE);
	return 1;
}

static int
get_pathvec_sysfs (vector pathvec)
{
	struct sysfs_directory * sdir;
	struct sysfs_directory * devp;
	struct sysfs_link * linkp;
	char refwwid[WWID_SIZE];
	char empty_buff[WWID_SIZE];
	char buff[FILE_NAME_SIZE];
	char path[FILE_NAME_SIZE];
	char sysfs_path[FILE_NAME_SIZE];
	struct path * curpath;

	memset (empty_buff, 0, WWID_SIZE);
	memset (refwwid, 0, WWID_SIZE);

	if (sysfs_get_mnt_path(sysfs_path, FILE_NAME_SIZE)) {
		fprintf(stderr, "multipath tools need sysfs\n");
		exit(1);
	}
	
	/*
	 * if called from /etc/dev.d or pathcheckers, only consider the paths
	 * that relate to the device pointed by conf->dev
	 */
	if (conf->dev && filepresent(conf->dev)) {
		curpath = zalloc(sizeof (struct path));
		basename(conf->dev, curpath->dev);

		if (devinfo(curpath))
			return 1;

		memcpy(refwwid, curpath->wwid, WWID_SIZE);
		free(curpath);
	}

	/*
	 * if devt specified on the cmd line,
	 * only consider affiliated paths
	 */
	if (conf->devt) {
		if (devt2devname(buff, conf->devt))
			return 1;
		
		curpath = zalloc(sizeof (struct path));
		if(safe_sprintf(curpath->dev, "%s", buff)) {
			fprintf(stderr, "curpath->dev too small\n");
			exit(1);
		}

		if (devinfo(curpath))
			return 1;

		memcpy(refwwid, curpath->wwid, WWID_SIZE);
		free(curpath);
	}
		
	if(safe_sprintf(path, "%s/block", sysfs_path)) {
		fprintf(stderr, "path too small\n");
		exit(1);
	}
	sdir = sysfs_open_directory(path);
	sysfs_read_directory(sdir);

	dlist_for_each_data(sdir->subdirs, devp, struct sysfs_directory) {
		if (blacklist(devp->name))
			continue;

		sysfs_read_directory(devp);

		if (devp->links == NULL)
			continue;

		dlist_for_each_data(devp->links, linkp, struct sysfs_link) {
			if (!strncmp(linkp->name, "device", 6))
				break;
		}
		if (linkp == NULL) {
			continue;
		}
		basename(devp->path, buff);
		curpath = zalloc(sizeof(struct path));

		if(safe_sprintf(curpath->dev, "%s", buff)) {
			fprintf(stderr, "curpath->dev too small\n");
			exit(1);
		}
		if (devinfo(curpath)) {
			free (curpath);
			continue;
		}

		if (memcmp(empty_buff, refwwid, WWID_SIZE) != 0 && 
		    memcmp(curpath->wwid, refwwid, WWID_SIZE) != 0) {
			free(curpath);
			continue;
		}

		basename(linkp->target, buff);
		sscanf(buff, "%i:%i:%i:%i",
			&curpath->sg_id.host_no,
			&curpath->sg_id.channel,
			&curpath->sg_id.scsi_id,
			&curpath->sg_id.lun);

		vector_alloc_slot(pathvec);
		vector_set_slot(pathvec, curpath);
	}
	sysfs_close_directory(sdir);
	return 0;
}

/*
 * print_path style
 */
#define ALL	0
#define SHORT	1

static void
print_path (struct path * pp, int style)
{
	if (style != SHORT)
		printf ("%s ", pp->wwid);
	else
		printf (" \\_");

	printf ("(%i %i %i %i) ",
	       pp->sg_id.host_no,
	       pp->sg_id.channel,
	       pp->sg_id.scsi_id,
	       pp->sg_id.lun);

	printf ("%s ", pp->dev);

	switch (pp->state) {
	case PATH_UP:
		printf ("[ready ] ");
		break;
	case PATH_DOWN:
		printf ("[faulty] ");
		break;
	case PATH_SHAKY:
		printf ("[shaky] ");
		break;
	default:
		dbg("undefined path state");
		break;
	}
	printf ("(%s) ", pp->dev_t);

	if (pp->claimed)
		printf ("[claimed] ");

	if (style != SHORT)
		printf ("[%.16s]", pp->product_id);

	fprintf (stdout, "\n");
}

static void
print_all_path (vector pathvec)
{
	int k;
	char empty_buff[WWID_SIZE];
	struct path * pp;

	/* initialize a cmp 0-filled buffer */
	memset (empty_buff, 0, WWID_SIZE);

	fprintf (stdout, "#\n# all paths :\n#\n");

	vector_foreach_slot (pathvec, pp, k) {
		/* leave out paths with incomplete devinfo */
		if (memcmp (empty_buff, pp->wwid, WWID_SIZE) == 0)
			continue;

		print_path (pp, ALL);
	}
}

static void
print_all_mp (vector mp)
{
	int k, i;
	struct multipath * mpp;
	struct path * pp = NULL;

	fprintf (stdout, "#\n# all multipaths :\n#\n");

	vector_foreach_slot (mp, mpp, k) {
		if (mpp->alias)
			printf ("%s (%s)", mpp->alias, mpp->wwid);
		else
			printf ("%s", mpp->wwid);

		pp = VECTOR_SLOT(mpp->paths, 0);
		printf (" [%.16s]\n", pp->product_id);

		vector_foreach_slot (mpp->paths, pp, i)
			print_path (pp, SHORT);
	}
}

static struct mpentry *
find_mp (char * wwid)
{
	int i;
	struct mpentry * mpe;

	vector_foreach_slot (conf->mptable, mpe, i)
                if (strcmp(mpe->wwid, wwid) == 0)
			return mpe;

	return NULL;
}

static void
coalesce_paths (vector mp, vector pathvec)
{
	int k, i, already_done;
	char empty_buff[WWID_SIZE];
	struct multipath * mpp;
	struct mpentry * mpe;
	struct path * pp1;
	struct path * pp2;

	already_done = 0;
	memset (empty_buff, 0, WWID_SIZE);

	vector_foreach_slot (pathvec, pp1, k) {
		/* skip this path for some reason */

		/* 1. if path has no unique id */
		if (memcmp (empty_buff, pp1->wwid, WWID_SIZE) == 0)
			continue;

		/* 2. if mp with this uid already instanciated */
		vector_foreach_slot (mp, mpp, i)
			if (0 == strcmp (mpp->wwid, pp1->wwid))
				already_done = 1;

		if (already_done) {
			already_done = 0;
			continue;
		}

		/*
		 * at this point, we know we really got a new mp
		 */
		vector_alloc_slot(mp);
		mpp = zalloc(sizeof(struct multipath));
		strcpy (mpp->wwid, pp1->wwid);

		mpe = find_mp(mpp->wwid);
		if (mpe)
			mpp->alias = mpe->alias;

		mpp->paths = vector_alloc();
		vector_alloc_slot (mpp->paths);
		vector_set_slot (mpp->paths, VECTOR_SLOT(pathvec, k));

		if (mpp->size == 0 && pp1->state == PATH_UP)
			mpp->size = get_disk_size(pp1->dev);

		for (i = k + 1; i < VECTOR_SIZE(pathvec); i++) {
			pp2 = VECTOR_SLOT(pathvec, i);
			if (0 == strcmp(pp1->wwid, pp2->wwid)) {
				vector_alloc_slot(mpp->paths);
				vector_set_slot(mpp->paths,
						VECTOR_SLOT(pathvec, i));

				if (mpp->size == 0 && pp2->state == PATH_UP)
					mpp->size = get_disk_size(pp2->dev);

			}
		}
		vector_set_slot(mp, mpp);
	}
}

static int
dm_simplecmd (int task, const char *name) {
	int r = 0;
	struct dm_task *dmt;

	if (!(dmt = dm_task_create(task)))
		return 0;

	if (!dm_task_set_name(dmt, name))
		goto out;

	r = dm_task_run(dmt);

	out:
		dm_task_destroy(dmt);
		return r;
}

static int
dm_addmap (int task, const char *name, const char *params, unsigned long size) {
	struct dm_task *dmt;

	if (!(dmt = dm_task_create(task)))
		return 0;

	if (!dm_task_set_name(dmt, name))
		goto out;

	if (!dm_task_add_target(dmt, 0, size, DM_TARGET, params))
		goto out;

	if (dm_task_run(dmt))
		return 0;

	out:
	dm_task_destroy(dmt);
	return 1;
}

static int
dm_map_present (char * str)
{
        int r = 0;
	struct dm_task *dmt;
        struct dm_names *names;
        unsigned next = 0;

	if (!(dmt = dm_task_create(DM_DEVICE_LIST)))
		return 0;

	if (!dm_task_run(dmt))
		goto out;

	if (!(names = dm_task_get_names(dmt)))
		goto out;

	if (!names->dev) {
		goto out;
	}

	do {
		if (0 == strncmp(names->name, str, strlen(names->name)))
			r = 1;
		next = names->next;
		names = (void *) names + next;
	} while (next);

	out:
	dm_task_destroy(dmt);
	return r;
}

static int
dm_get_map(char * name, char * outparams)
{
	int r = 0;
	struct dm_task *dmt;
	void *next = NULL;
	uint64_t start, length;
	char *target_type = NULL;
	char *params;
	int cmd;

	cmd = DM_DEVICE_TABLE;

	if (!(dmt = dm_task_create(cmd)))
		return 0;

	if (!dm_task_set_name(dmt, name))
		goto out;

	if (!dm_task_run(dmt))
		goto out;

	/* Fetch 1st target */
	next = dm_get_next_target(dmt, next, &start, &length,
				  &target_type, &params);

	if (safe_snprintf(outparams, PARAMS_SIZE, params))
		goto out;

	r = 1;

	out:
	dm_task_destroy(dmt);
	return r;
}

static int
dm_reinstate(char * mapname, char * path)
{
        int r = 0;
	int sz;
        struct dm_task *dmt;
        char *str;

        if (!(dmt = dm_task_create(DM_DEVICE_TARGET_MSG)))
                return 0;

        if (!dm_task_set_name(dmt, mapname))
                goto out;

        if (!dm_task_set_sector(dmt, 0))
                goto out;

	sz = strlen(path) + 11;
        str = zalloc(sz);

	snprintf(str, sz, "reinstate %s\n", path);

        if (!dm_task_set_message(dmt, str))
                goto out;

        free(str);

        if (!dm_task_run(dmt))
                goto out;

        r = 1;

      out:
        dm_task_destroy(dmt);

        return r;
}

static int
select_selector (struct multipath * mp)
{
	int i;
	struct mpentry * mpe;
	struct hwentry * hwe;
	struct path * pp;

	pp = VECTOR_SLOT(mp->paths, 0);

	/*
	 * select the right path selector :
	 * 1) set internal default
	 * 2) override by controler wide settings
	 * 3) override by LUN specific settings
	 */

	/* 1) set internal default */
	mp->selector = conf->default_selector;
	mp->selector_args = conf->default_selector_args;

	/* 2) override by controler wide settings */
	vector_foreach_slot (conf->hwtable, hwe, i) {
		if (strcmp(hwe->vendor, pp->vendor_id) == 0 &&
		    strcmp(hwe->product, pp->product_id) == 0) {
			mp->selector = (hwe->selector) ?
				   hwe->selector : conf->default_selector;
			mp->selector_args = (hwe->selector_args) ?
				   hwe->selector_args : conf->default_selector_args;
		}
	}

	/* 3) override by LUN specific settings */
	vector_foreach_slot (conf->mptable, mpe, i) {
		if (strcmp(mpe->wwid, mp->wwid) == 0) {
			mp->selector = (mpe->selector) ?
				   mpe->selector : conf->default_selector;
			mp->selector_args = (mpe->selector_args) ?
				   mpe->selector_args : conf->default_selector_args;
		}
	}
	return 0;
}

/*
 * Transforms the path group vector into a proper device map string
 */
void
assemble_map (struct multipath * mp)
{
	int i, j;
	int shift, freechar;
	char * p;
	vector pgpaths;
	struct path * pp;

	p = mp->params;
	freechar = sizeof(mp->params);
	
	shift = snprintf(p, freechar, "%i", VECTOR_SIZE(mp->pg));

	if (shift >= freechar) {
		fprintf(stderr, "mp->params too small\n");
		exit(1);
	}
	p += shift;
	freechar -= shift;
	
	for (i = VECTOR_SIZE(mp->pg) - 1; i >= 0; i--) {
		pgpaths = VECTOR_SLOT(mp->pg, i);
		shift = snprintf(p, freechar, " %s %i %i", mp->selector,
				 VECTOR_SIZE(pgpaths), mp->selector_args);
		if (shift >= freechar) {
			fprintf(stderr, "mp->params too small\n");
			exit(1);
		}
		p += shift;
		freechar -= shift;

		vector_foreach_slot (pgpaths, pp, j) {
			shift = snprintf(p, freechar, " %s", pp->dev_t);
			if (shift >= freechar) {
				fprintf(stderr, "mp->params too small\n");
				exit(1);
			}
			p += shift;
			freechar -= shift;
		}
	}
	if (freechar < 1) {
		fprintf(stderr, "mp->params too small\n");
		exit(1);
	}
	snprintf(p, 1, "\n");
}

static int
setup_map (vector pathvec, struct multipath * mpp)
{
	struct path * pp;
	int i, iopolicy;
	char iopolicy_name[POLICY_NAME_SIZE];
	int fd;
	struct hwentry * hwe = NULL;
	struct mpentry * mpe;
	char * mapname;
	char curparams[PARAMS_SIZE];
	int op;

	/*
	 * don't bother if devmap size is unknown
	 */
	if (mpp->size <= 0)
		return 0;

	/*
	 * don't bother if a constituant path is claimed
	 * FIXME : claimed detection broken, always unclaimed for now
	 */
	vector_foreach_slot (mpp->paths, pp, i)
		if (pp->claimed)
			return 0;

	pp = VECTOR_SLOT(mpp->paths, 0);

	/*
	 * iopolicy selection logic :
	 *  1) set internal default
	 *  2) override by config file default
	 *  3) override by controler wide setting
	 *  4) override by LUN specific setting
	 *  5) cmd line flag has the last word
	 */
	dbg("iopolicy selector :");
	dbg("===================");

	/*
	 * 1) set internal default
	 */
	iopolicy = FAILOVER;
	get_pgpolicy_name(iopolicy_name, FAILOVER);
	dbg("internal default)\tiopolicy = %s", iopolicy_name);

	/*
	 * 2) override by config file default
	 */
	if (conf->default_iopolicy > 0) {
		iopolicy = conf->default_iopolicy;
		get_pgpolicy_name(iopolicy_name, iopolicy);
		dbg("config file default)\tiopolicy = %s", iopolicy_name);
	}
	
	/*
	 * 3) override by controler wide setting
	 */
	vector_foreach_slot (conf->hwtable, hwe, i) {
		if (MATCH (pp->vendor_id, hwe->vendor) &&
		    MATCH (pp->product_id, hwe->product) &&
		    hwe->iopolicy > 0) {
			iopolicy = hwe->iopolicy;
			get_pgpolicy_name(iopolicy_name, iopolicy);
			dbg("controler override)\tiopolicy = %s", iopolicy_name);
		}
	}

	/*
	 * 4) override by LUN specific setting
	 */
	vector_foreach_slot (conf->mptable, mpe, i) {
		if (strcmp(mpe->wwid, mpp->wwid) == 0 &&
		    mpe->iopolicy > 0) {
			iopolicy = mpe->iopolicy;
			get_pgpolicy_name(iopolicy_name, iopolicy);
			dbg("lun override)\t\tiopolicy = %s", iopolicy_name);
		}
	}

	/*
	 * 5) cmd line flag has the last word
	 */
	if (conf->iopolicy_flag > 0) {
		iopolicy = conf->iopolicy_flag;
		get_pgpolicy_name(iopolicy_name, iopolicy);
		dbg("cmd flag override)\tiopolicy = %s", iopolicy_name);
	}

	/*
	 * select the appropriate path group selector and args
	 */
	select_selector(mpp);

	/*
	 * layered map computation :
	 *  1) separate failed paths in a tersary PG
	 *  2) separate shaky paths in a secondary PG
	 *  3) apply selected grouping policy to valid paths
	 *  4) reorder path groups by summed priority
	 */

	/*
	 * 1) & 2)
	 */
	group_by_status(mpp, PATH_DOWN);
	group_by_status(mpp, PATH_SHAKY);

	/*
	 * 3) apply selected grouping policy
	 */
	if (iopolicy == MULTIBUS)
		one_group (mpp);

	if (iopolicy == FAILOVER)
		one_path_per_group (mpp);

	if (iopolicy == GROUP_BY_SERIAL)
		group_by_serial (mpp);

	if (iopolicy == GROUP_BY_PRIO)
		group_by_prio (mpp);

	if (mpp->pg == NULL) {
		dbg("iopolicy failed to produce a ->pg vector");
		return 1;
	}

	/*
	 * 4)
	 */
	sort_pg_by_summed_prio(mpp);

	/*
	 * transform the mp->pg vector of vectors of paths
	 * into a mp->params strings to feed the device-mapper
	 */
	assemble_map(mpp);

	/*
	 * select between RELOAD and CREATE
	 */
	mapname = mpp->alias ? mpp->alias : mpp->wwid;
	op = dm_map_present(mapname) ? DM_DEVICE_RELOAD : DM_DEVICE_CREATE;
	
	if (conf->verbosity > 1)
		printf("%s:", (op == DM_DEVICE_RELOAD) ? "reload" : "create");
	if (conf->verbosity > 0)
		printf("%s", mapname);
	if (conf->verbosity > 1)
		printf(":0 %lu %s %s",
			mpp->size, DM_TARGET,
			mpp->params);
	if (conf->verbosity > 0)
		printf("\n");

	/*
	 * last chance to quit before touching the devmaps
	 */
	if (conf->dry_run)
		return 1;

	/*
	 * device mapper creation or updating
	 * here we know we'll have garbage on stderr from
	 * libdevmapper. so shut it down temporarily.
	 */
	fd = dup(2);
	close(2);

	if (op == DM_DEVICE_RELOAD) {
		if (conf->dev && filepresent(conf->dev) &&
		    dm_get_map(mapname, curparams) &&
		    0 == strncmp(mpp->params, curparams, strlen(mpp->params))) {
	                pp = zalloc(sizeof(struct path));
	                basename(conf->dev, pp->dev);

	                if (devinfo(pp))
	                        return 1;

			dm_reinstate(mapname, pp->dev_t);
	                free(pp);

			if (conf->verbosity > 1)
				printf("[reinstate %s in %s]\n",
					conf->dev, mapname);
			goto out;
		}
		if (!dm_simplecmd(DM_DEVICE_SUSPEND, mapname))
			goto out;
	}

	if (dm_addmap(op, mapname, mpp->params, mpp->size)) {
		dm_simplecmd(DM_DEVICE_REMOVE, mapname);
		goto out;
	}

	if (op == DM_DEVICE_RELOAD)
		if (!dm_simplecmd(DM_DEVICE_RESUME, mapname))
			goto out;

	return 1;

	out:
	dup2(fd, 2);
	close(fd);
	return 0;
}

static void
signal_daemon (void)
{
	FILE *file;
	pid_t pid;
	char *buf;

	buf = malloc (8);

	file = fopen (PIDFILE, "r");

	if (!file) {
		if (conf->verbosity > 0)
			fprintf (stderr, "cannot signal daemon, "
					 "pidfile not found\n");
		return;
	}

	buf = fgets (buf, 8, file);
	fclose (file);

	pid = (pid_t) atol (buf);
	free (buf);

	kill (pid, SIGHUP);
}

#define VECTOR_ADDSTR(a, b) \
	str = zalloc (6 * sizeof(char)); \
	snprintf (str, 6, b); \
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

#define VECTOR_ADDHWE(a, b, c, d, e) \
	hwe = zalloc (sizeof(struct hwentry)); \
	hwe->vendor = zalloc (SCSI_VENDOR_SIZE * sizeof(char)); \
	snprintf (hwe->vendor, SCSI_VENDOR_SIZE, b); \
	hwe->product = zalloc (SCSI_PRODUCT_SIZE * sizeof(char)); \
	snprintf (hwe->product, SCSI_PRODUCT_SIZE, c); \
	hwe->iopolicy = d; \
	hwe->getuid = e; \
	vector_alloc_slot(a); \
	vector_set_slot(a, hwe);
	
static void
setup_default_hwtable (vector hwtable)
{
	struct hwentry * hwe;
	
	VECTOR_ADDHWE(hwtable, "COMPAQ", "HSV110 (C)COMPAQ", MULTIBUS, "/bin/scsi_id -g -s");
	VECTOR_ADDHWE(hwtable, "COMPAQ", "MSA1000", MULTIBUS, "/bin/scsi_id -g -s");
	VECTOR_ADDHWE(hwtable, "COMPAQ", "MSA1000 VOLUME", MULTIBUS, "/bin/scsi_id -g -s");
	VECTOR_ADDHWE(hwtable, "DEC", "HSG80", MULTIBUS, "/bin/scsi_id -g -s");
	VECTOR_ADDHWE(hwtable, "HP", "HSV110", MULTIBUS, "/bin/scsi_id -g -s");
	VECTOR_ADDHWE(hwtable, "HP", "A6189A", MULTIBUS, "/bin/scsi_id -g -s");
	VECTOR_ADDHWE(hwtable, "HP", "OPEN-", MULTIBUS, "/bin/scsi_id -g -s");
	VECTOR_ADDHWE(hwtable, "DDN", "SAN DataDirector", MULTIBUS, "/bin/scsi_id -g -s");
	VECTOR_ADDHWE(hwtable, "FSC", "CentricStor", MULTIBUS, "/bin/scsi_id -g -s");
	VECTOR_ADDHWE(hwtable, "HITACHI", "DF400", MULTIBUS, "/bin/scsi_id -g -s");
	VECTOR_ADDHWE(hwtable, "HITACHI", "DF500", MULTIBUS, "/bin/scsi_id -g -s");
	VECTOR_ADDHWE(hwtable, "HITACHI", "DF600", MULTIBUS, "/bin/scsi_id -g -s");
	VECTOR_ADDHWE(hwtable, "IBM", "ProFibre 4000R", MULTIBUS, "/bin/scsi_id -g -s");
	VECTOR_ADDHWE(hwtable, "SGI", "TP9100", MULTIBUS, "/bin/scsi_id -g -s");
	VECTOR_ADDHWE(hwtable, "SGI", "TP9300", MULTIBUS, "/bin/scsi_id -g -s");
	VECTOR_ADDHWE(hwtable, "SGI", "TP9400", MULTIBUS, "/bin/scsi_id -g -s");
	VECTOR_ADDHWE(hwtable, "SGI", "TP9500", MULTIBUS, "/bin/scsi_id -g -s");
	VECTOR_ADDHWE(hwtable, "3PARdata", "VV", MULTIBUS, "/bin/scsi_id -g -s");
	VECTOR_ADDHWE(hwtable, "STK", "OPENstorage D280", GROUP_BY_SERIAL, "/bin/scsi_id -g -s");
	VECTOR_ADDHWE(hwtable, "SUN", "StorEdge 3510", MULTIBUS, "/bin/scsi_id -g -s");
	VECTOR_ADDHWE(hwtable, "SUN", "T4", MULTIBUS, "/bin/scsi_id -g -s");
}

static void
usage (char * progname)
{
	fprintf (stderr, VERSION_STRING);
	fprintf (stderr, "Usage: %s\t[-v level] [-d] [-D major:minor] [-S]\n",
		progname);
	fprintf (stderr,
		"\t\t\t[-p failover|multibus|group_by_serial|group_by_prio]\n" \
		"\t\t\t[device]\n" \
		"\n" \
		"\t-v level\tverbosty level\n" \
		"\t   0\t\t\tno output\n" \
		"\t   1\t\t\tprint created devmap names only\n" \
		"\t   2\t\t\tprint all paths and multipaths\n" \
		"\t-d\t\tdry run, do not create or update devmaps\n" \
		"\t-D maj:min\tlimit scope to the device's multipath\n" \
		"\t\t\t(major:minor device reference)\n"
		"\t-S\t\tinhibit signal sending to multipathd\n"
		"\n" \
		"\t-p policy\tforce all maps to specified policy :\n" \
		"\t   failover\t\t1 path per priority group\n" \
		"\t   multibus\t\tall paths in 1 priority group\n" \
		"\t   group_by_serial\t1 priority group per serial\n" \
		"\t   group_by_prio\t1 priority group per priority lvl\n" \
		"\n" \
		"\tdevice\t\tlimit scope to the device's multipath\n" \
		"\t\t\t(udev-style $DEVNAME reference, eg /dev/sdb\n" \
		"\t\t\tor a device map name)\n" \
		);

	exit(1);
}

int try_lock (char * file)
{
	int fd;
	struct flock fl;

	/*
	 * create the file to lock if it does not exist
	 */
	fd = open(file, O_CREAT|O_RDWR);

	if (fd < 0) {
		fprintf(stderr, "can't create runfile\n");
		exit(1);
	}
	fl.l_type = F_WRLCK;
	fl.l_whence = 0;
	fl.l_start = 0;
	fl.l_len = 0;

	/*
	 * set a max wait time
	 */
	alarm(2);
	
	if (fcntl(fd, F_SETLKW, &fl) == -1) {
		fprintf(stderr, "can't take a write lease on %s\n", file);
		return 1;
	}
	alarm(0);

	return 0;
}

int
main (int argc, char *argv[])
{
	vector mp;
	vector pathvec;
	struct multipath * mpp;
	int k;
	int arg;
	extern char *optarg;
	extern int optind;

	/*
	 * Don't run in parallel
	 * ie, acquire a F_WRLCK-type lock on RUN
	 */
	if (try_lock(RUN)) {
		fprintf(stderr, "waited for to long. exiting\n");
		exit(1);
	}
	
	/*
	 * alloc config struct
	 */
	conf = zalloc(sizeof(struct config));
				
	/*
	 * internal defaults
	 */
	conf->dry_run = 0;		/* 1 == Do not Create/Update devmaps */
	conf->verbosity = 1;		/* 1 == Print mp names */
	conf->iopolicy_flag = 0;	/* do not override defaults */
	conf->signal = 1;		/* 1 == Send a signal to multipathd */
	conf->dev = NULL;
	conf->devt = NULL;
	conf->default_selector = NULL;
	conf->default_selector_args = 0;
	conf->default_iopolicy = 0;
	conf->mptable = NULL;
	conf->hwtable = NULL;
	conf->blist = NULL;

	while ((arg = getopt(argc, argv, ":qdSi:v:p:D:")) != EOF ) {
		switch(arg) {
		case 1: printf("optarg : %s\n",optarg);
			break;
		case 'v':
			if (sizeof(optarg) > sizeof(char *) ||
			    !isdigit(optarg[0]))
				usage (argv[0]);

			conf->verbosity = atoi(optarg);
			break;
		case 'd':
			conf->dry_run = 1;
			conf->signal = 0;
			break;
		case 'S':
			conf->signal = 0;
			break;
		case 'p':
			if (strcmp(optarg, "failover") == 0)
				conf->iopolicy_flag = FAILOVER;
			else if (strcmp(optarg, "multibus") == 0)
				conf->iopolicy_flag = MULTIBUS;
			else if (strcmp(optarg, "group_by_serial") == 0)
				conf->iopolicy_flag = GROUP_BY_SERIAL;
			else if (strcmp(optarg, "group_by_prio") == 0)
				conf->iopolicy_flag = GROUP_BY_PRIO;
			else
			{
				if (optarg)
					printf("'%s' is not a valid "
						"policy\n", optarg);
				usage(argv[0]);
			}                
			break;
		case 'D':
			conf->devt = zalloc(DEV_T_SIZE);
			strncpy(conf->devt, optarg, DEV_T_SIZE);
			break;
		case ':':
			fprintf(stderr, "Missing option arguement\n");
			usage(argv[0]);        
		case '?':
			fprintf(stderr, "Unknown switch: %s\n", optarg);
			usage(argv[0]);
		default:
			usage(argv[0]);
		}
	}        
	if (optind<argc) {
		conf->dev = zalloc(FILE_NAME_SIZE);
		strncpy(conf->dev, argv[optind], FILE_NAME_SIZE);
	}

	/*
	 * read the config file
	 */
	if (filepresent(CONFIGFILE))
		init_data (CONFIGFILE, init_keywords);
	
	/*
	 * fill the voids left in the config file
	 */
	if (conf->hwtable == NULL) {
		conf->hwtable = vector_alloc();
		setup_default_hwtable(conf->hwtable);
	}
	if (conf->blist == NULL) {
		conf->blist = vector_alloc();
		setup_default_blist(conf->blist);
	}
	if (conf->mptable == NULL)
		conf->mptable = vector_alloc();

	if (conf->default_selector == NULL)
		conf->default_selector = DEFAULT_SELECTOR;

	if (conf->udev_dir == NULL)
		conf->udev_dir = DEFAULT_UDEVDIR;

	if (conf->default_getuid == NULL)
		conf->default_getuid = DEFAULT_GETUID;

	/*
	 * allocate the two core vectors to store paths and multipaths
	 */
	mp = vector_alloc();
	pathvec = vector_alloc();

	if (mp == NULL || pathvec == NULL) {
		fprintf(stderr, "can not allocate memory\n");
		exit(1);
	}

	if (get_pathvec_sysfs(pathvec))
		exit(1);

	if (VECTOR_SIZE(pathvec) == 0 && conf->verbosity > 0) {
		fprintf(stdout, "no path found\n");
		exit(0);
	}

	coalesce_paths(mp, pathvec);

	/*
	 * may be conf->dev is a mapname
	 * if so, only reconfigure this map
	 */
	vector_foreach_slot (mp, mpp, k) {
		if (conf->dev && (
		    (mpp->alias &&
		    0 == strncmp(mpp->alias, conf->dev, FILE_NAME_SIZE)) ||
		    0 == strncmp(mpp->wwid, conf->dev, FILE_NAME_SIZE))) {
			setup_map(pathvec, mpp);
			goto out;
		}
	}

	if (conf->verbosity > 1) {
		print_all_path(pathvec);
		print_all_mp(mp);
	}

	if (conf->verbosity > 1)
		fprintf(stdout, "#\n# device maps :\n#\n");

	vector_foreach_slot (mp, mpp, k)
		setup_map(pathvec, mpp);

out:
	/*
	 * signal multipathd that new devmaps may have come up
	 */
	if (conf->signal)
		signal_daemon();
	
	/*
	 * free allocs
	 */
	free(mp);
	free(pathvec);

	exit(0);
}
