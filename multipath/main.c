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
#include <devmapper.h>
#include <checkers.h>
#include <path_state.h>
#include <safe_printf.h>
#include <callout.h>

#include "main.h"
#include "devinfo.h"
#include "config.h"
#include "pgpolicies.h"
#include "dict.h"
#include "debug.h"

/* helpers */
#define argis(x) if (0 == strcmp (x, argv[i]))
#define MATCH(x,y) 0 == strncmp (x, y, strlen(y))

static struct hwentry *
find_hw (char * vendor, char * product)
{
	int i;
	struct hwentry * hwe;

	vector_foreach_slot (conf->hwtable, hwe, i)
		if (hwe->vendor && hwe->product &&
		    strcmp(hwe->vendor, vendor) == 0 &&
		    strcmp(hwe->product, product) == 0)
			return hwe;
	return NULL;
}

static struct mpentry *
find_mp (char * wwid)
{
	int i;
	struct mpentry * mpe;

	vector_foreach_slot (conf->mptable, mpe, i)
                if (mpe->wwid && strcmp(mpe->wwid, wwid) == 0)
			return mpe;

	return NULL;
}

/*
 * selectors :
 * traverse the configuration layers from most specific to most generic
 * stop at first explicit setting found
 */
static int
select_pgpolicy (struct multipath * mp)
{
	int i;
	struct mpentry * mpe = NULL;
	struct hwentry * hwe = NULL;
	struct path * pp;
	char pgpolicy_name[POLICY_NAME_SIZE];

	pp = VECTOR_SLOT(mp->paths, 0);

	if (conf->pgpolicy_flag > 0) {
		mp->pgpolicy = conf->pgpolicy_flag;
		get_pgpolicy_name(pgpolicy_name, mp->pgpolicy);
		dbg("pgpolicy = %s (cmd line flag)", pgpolicy_name);
		return 0;
	}
	vector_foreach_slot (conf->mptable, mpe, i) {
		if (mpe->wwid && strcmp(mpe->wwid, mp->wwid) == 0 &&
		    mpe->pgpolicy > 0) {
			mp->pgpolicy = mpe->pgpolicy;
			get_pgpolicy_name(pgpolicy_name, mp->pgpolicy);
			dbg("pgpolicy = %s (LUN setting)", pgpolicy_name);
			return 0;
		}
	}
	vector_foreach_slot (conf->hwtable, hwe, i) {
		if (hwe->vendor && hwe->product &&
		    MATCH (pp->vendor_id, hwe->vendor) &&
		    MATCH (pp->product_id, hwe->product) &&
		    hwe->pgpolicy > 0) {
			mp->pgpolicy = hwe->pgpolicy;
			get_pgpolicy_name(pgpolicy_name, mp->pgpolicy);
			dbg("pgpolicy = %s (controler setting)", pgpolicy_name);
			return 0;
		}
	}
	if (conf->default_pgpolicy > 0) {
		mp->pgpolicy = conf->default_pgpolicy;
		get_pgpolicy_name(pgpolicy_name, mp->pgpolicy);
		dbg("pgpolicy = %s (config file default)", pgpolicy_name);
		return 0;
	}
	mp->pgpolicy = FAILOVER;
	get_pgpolicy_name(pgpolicy_name, FAILOVER);
	dbg("pgpolicy = %s (internal default)", pgpolicy_name);
	return 0;
}

static int
select_selector (struct multipath * mp)
{
	int i;
	struct mpentry * mpe = NULL;
	struct hwentry * hwe = NULL;
	struct path * pp;

	pp = VECTOR_SLOT(mp->paths, 0);

	mpe = find_mp(mp->wwid);

	if (mpe && mpe->selector) {
		mp->selector = mpe->selector;
		mp->selector_args = mpe->selector_args;
		dbg("selector = %s (LUN setting)", mp->selector);
		dbg("selector_args = %s (LUN setting)", mp->selector_args);
		return 0;
	}
	hwe = find_hw(pp->vendor_id, pp->product_id);

	if (hwe && hwe->selector) {
		mp->selector = hwe->selector;
		mp->selector_args = hwe->selector_args;
		dbg("selector = %s (controler setting)", mp->selector);
		dbg("selector_args = %s (controler setting)", mp->selector_args);
		return 0;
	}
	mp->selector = conf->default_selector;
	mp->selector_args = conf->default_selector_args;
	dbg("selector = %s (internal default)", mp->selector);
	dbg("selector_args = %s (internal default)", mp->selector_args);
	return 0;
}

static int
select_features (struct multipath * mp)
{
	int i;
	struct hwentry * hwe = NULL;
	struct path * pp;

	pp = VECTOR_SLOT(mp->paths, 0);
	hwe = find_hw(pp->vendor_id, pp->product_id);

	if (hwe && hwe->features) {
		mp->features = hwe->features;
		dbg("features = %s (controler setting)", mp->features);
		return 0;
	}
	mp->features = conf->default_features;
	dbg("features = %s (internal default)", mp->features);
	return 0;
}

static int
select_hwhandler (struct multipath * mp)
{
	int i;
	struct hwentry * hwe = NULL;
	struct path * pp;

        pp = VECTOR_SLOT(mp->paths, 0);
	hwe = find_hw(pp->vendor_id, pp->product_id);

	if (hwe && hwe->hwhandler) {
		mp->hwhandler = hwe->hwhandler;
		dbg("hwhandler = %s (controler setting)", mp->hwhandler);
		return 0;
	}
	mp->hwhandler = conf->default_hwhandler;
	dbg("hwhandler = %s (internal default)", mp->hwhandler);
	return 0;
}

static int
select_checkfn(struct path *pp)
{
	char checker_name[CHECKER_NAME_SIZE];
	int i;
	struct hwentry * hwe = NULL;

	hwe = find_hw(pp->vendor_id, pp->product_id);

	if (hwe && hwe->checker_index > 0) {
		get_checker_name(checker_name, hwe->checker_index);
		dbg("path checker = %s (controler setting)", checker_name);
		pp->checkfn = get_checker_addr(hwe->checker_index);
		return 0;
	}
	pp->checkfn = &readsector0;
	get_checker_name(checker_name, READSECTOR0);
	dbg("path checker = %s (internal default)", checker_name);
	return 0;
}

static int
select_getuid (struct path * pp)
{
	int i;
	struct hwentry * hwe = NULL;

	hwe = find_hw(pp->vendor_id, pp->product_id);

	if (hwe && hwe->getuid) {
		pp->getuid = hwe->getuid;
		dbg("getuid = %s (controler setting)", pp->getuid);
		return 0;
	}
	pp->getuid = conf->default_getuid;
	dbg("getuid = %s (internal default)", pp->getuid);
	return 0;
}

static int
select_getprio (struct path * pp)
{
	int i;
	struct hwentry * hwe = NULL;

	hwe = find_hw(pp->vendor_id, pp->product_id);

	if (hwe && hwe->getprio) {
		pp->getprio = hwe->getprio;
		dbg("getprio = %s (controler setting)", pp->getprio);
		return 0;
	}
	pp->getprio = conf->default_getprio;
	dbg("getprio = %s (internal default)", pp->getprio);
	return 0;
}

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

static char *
apply_format (char * string, int maxsize, struct path * pp)
{
	char * pos;
	char * dst;
	char * p;
	char c;
	int len;
	int free;

	dst = zalloc(maxsize);

	if (!dst)
		return NULL;

	p = dst;
	pos = strchr(string, '%');
	free = maxsize;

	if (!pos)
		return string;

	len = (int) (pos - string) + 1;
	free -= len;

	if (free < 2)
		return NULL;

	snprintf(p, len, "%s", string);
	p += len - 1;
	pos++;

	switch (*pos) {
	case 'n':
		len = strlen(pp->dev) + 1;
		free -= len;

		if (free < 2)
			return NULL;

		snprintf(p, len, "%s", pp->dev);
		p += len - 1;
		break;
	case 'd':
		len = strlen(pp->dev_t) + 1;
		free -= len;

		if (free < 2)
			return NULL;

		snprintf(p, len, "%s", pp->dev_t);
		p += len - 1;
		break;
	default:
		break;
	}
	pos++;

	if (!*pos)
		return dst;

	len = strlen(pos) + 1;
	free -= len;

	if (free < 2)
		return NULL;

	snprintf(p, len, "%s", pos);
	dbg("reformated callout = %s", dst);
	return dst;
}

static int
devinfo (struct path *pp)
{
	int i;
	struct hwentry * hwe;
	char * buff;
	char prio[16];

	dbg("===== path %s =====", pp->dev);

	/*
	 * fetch info available in sysfs
	 */
	if (sysfs_devinfo(pp))
		return 1;

	/*
	 * then those not available through sysfs
	 */
	get_serial(pp->serial, pp->dev_t);
	dbg("serial = %s", pp->serial);
	pp->claimed = get_claimed(pp->dev_t);
	dbg("claimed = %i", pp->claimed);

	/*
	 * get path state, no message collection, no context
	 */
	select_checkfn(pp);
	pp->state = pp->checkfn(pp->dev_t, NULL, NULL);
	dbg("state = %i", pp->state);
	
	/*
	 * get path prio
	 */
	select_getprio(pp);
	buff = apply_format(pp->getprio, CALLOUT_MAX_SIZE, pp);

	if (!buff)
		pp->priority = 1;
	else if (execute_program(buff, prio, 16)) {
		dbg("error calling out %s", buff);
		pp->priority = 1;
	} else
		pp->priority = atoi(prio);

	dbg("prio = %u", pp->priority);

	/*
	 * get path uid
	 */
	select_getuid(pp);

	buff = apply_format(pp->getuid, CALLOUT_MAX_SIZE, pp);

	if (!buff)
		goto fallback;

	if (execute_program(buff, pp->wwid, WWID_SIZE) == 0) {
		dbg("uid = %s (callout)", pp->wwid);
		return 0;
	}

	fallback:
	if (!get_evpd_wwid(pp->dev_t, pp->wwid)) {
		dbg("uid = %s (internal getuid)", pp->wwid);
		return 0;
	}
	/*
	 * no wwid : blank for safety
	 */
	dbg("uid = 0x0 (unable to fetch)");
	memset(pp->wwid, 0, WWID_SIZE);
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
	if (style != SHORT && pp->wwid)
		printf ("%s ", pp->wwid);
	else
		printf (" \\_");

	printf("(%i %i %i %i) ",
	       pp->sg_id.host_no,
	       pp->sg_id.channel,
	       pp->sg_id.scsi_id,
	       pp->sg_id.lun);

	if (pp->dev)
		printf("%s ", pp->dev);

	switch (pp->state) {
	case PATH_UP:
		printf("[ready ]");
		break;
	case PATH_DOWN:
		printf("[faulty]");
		break;
	case PATH_SHAKY:
		printf("[shaky ]");
		break;
	default:
		printf("[undef ]");
		break;
	}
	if (pp->dev_t)
		printf("[%s]", pp->dev_t);

	if (pp->claimed)
		printf("[claimed]");

	if (style != SHORT && pp->product_id)
		printf("[%.16s]", pp->product_id);

	fprintf(stdout, "\n");
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

	fprintf(stdout, "#\n# all multipaths :\n#\n");

	vector_foreach_slot (mp, mpp, k) {
		if (mpp->alias)
			printf("%s (%s)", mpp->alias, mpp->wwid);
		else
			printf("%s", mpp->wwid);

		if (!mpp->paths) {
			printf("\n");
			continue;
		}
		pp = VECTOR_SLOT(mpp->paths, 0);

		if (pp->product_id)
			printf(" [%.16s]\n", pp->product_id);

		vector_foreach_slot (mpp->paths, pp, i)
			print_path(pp, SHORT);
	}
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
	
	shift = snprintf(p, freechar, "%s %s %i 1",
			 mp->features, mp->hwhandler,
			 VECTOR_SIZE(mp->pg));

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
	int i;
	char * mapname = NULL;
	char * curparams = NULL;
	int op;
	int r = 0;

	/*
	 * don't bother if devmap size is unknown
	 */
	if (mpp->size <= 0)
		return 1;

	/*
	 * don't bother if a constituant path is claimed
	 * FIXME : claimed detection broken, always unclaimed for now
	 */
	vector_foreach_slot (mpp->paths, pp, i)
		if (pp->claimed)
			return 1;

	pp = VECTOR_SLOT(mpp->paths, 0);

	/*
	 * properties selectors
	 */
	select_pgpolicy(mpp);
	select_selector(mpp);
	select_features(mpp);
	select_hwhandler(mpp);

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
	//group_by_status(mpp, PATH_DOWN);
	//group_by_status(mpp, PATH_SHAKY);

	/*
	 * 3) apply selected grouping policy
	 */
	if (mpp->pgpolicy == MULTIBUS)
		one_group (mpp);

	if (mpp->pgpolicy == FAILOVER)
		one_path_per_group (mpp);

	if (mpp->pgpolicy == GROUP_BY_SERIAL)
		group_by_serial (mpp);

	if (mpp->pgpolicy == GROUP_BY_PRIO)
		group_by_prio (mpp);

	if (mpp->pg == NULL) {
		dbg("pgpolicy failed to produce a ->pg vector");
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
		    dm_get_map(mapname, NULL, &curparams) &&
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

	if (dm_addmap(op, mapname, DM_TARGET, mpp->params, mpp->size)) {
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
	hwe->pgpolicy = d; \
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

	if (dm_prereq(DM_TARGET, 0, 0, 0)) {
		dbg("device mapper prerequisites not met");
		exit(1);
	}

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
	conf->list = 0;
	conf->dry_run = 0;		/* 1 == Do not Create/Update devmaps */
	conf->verbosity = 1;
	conf->pgpolicy_flag = 0;	/* do not override defaults */
	conf->signal = 1;		/* 1 == Send a signal to multipathd */
	conf->dev = NULL;
	conf->devt = NULL;
	conf->default_selector = NULL;
	conf->default_selector_args = 0;
	conf->default_pgpolicy = 0;
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
				conf->pgpolicy_flag = FAILOVER;
			else if (strcmp(optarg, "multibus") == 0)
				conf->pgpolicy_flag = MULTIBUS;
			else if (strcmp(optarg, "group_by_serial") == 0)
				conf->pgpolicy_flag = GROUP_BY_SERIAL;
			else if (strcmp(optarg, "group_by_prio") == 0)
				conf->pgpolicy_flag = GROUP_BY_PRIO;
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

	if (conf->default_features == NULL)
		conf->default_features = DEFAULT_FEATURES;

	if (conf->default_hwhandler == NULL)
		conf->default_hwhandler = DEFAULT_HWHANDLER;

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
