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
#include <dlist.h>
#include <sysfs/libsysfs.h>

#include "../libdevmapper/libdevmapper.h"
#include "main.h"
#include "devinfo.h"
#include "config.h"
#include "pgpolicies.h"
#include "parser.h"
#include "dict.h"
#include "vector.h"
#include "memory.h"
#include "callout.h"
#include "debug.h"

/* helpers */
#define argis(x) if (0 == strcmp (x, argv[i]))
#define MATCH(x,y) 0 == strncmp (x, y, strlen(y))

static int
exit_tool (int status)
{
	unlink(RUN);
	exit(status);
}
	
static int
devt2devname (char *devname, int major, int minor)
{
	struct sysfs_directory * sdir;
	struct sysfs_directory * devp;
	char sysfs_path[FILE_NAME_SIZE];
	char block_path[FILE_NAME_SIZE];
	char attr_path[FILE_NAME_SIZE];
	char attr_value[16];
	char attr_ref_value[16];

	if (sysfs_get_mnt_path (sysfs_path, FILE_NAME_SIZE)) {
		fprintf (stderr, "-D feature available with sysfs only\n");
		exit (1);
	}
		
	sprintf (attr_ref_value, "%i:%i\n", major, minor);
	sprintf (block_path, "%s/block", sysfs_path);
	sdir = sysfs_open_directory (block_path);
	sysfs_read_directory (sdir);

	dlist_for_each_data (sdir->subdirs, devp, struct sysfs_directory) {
		sprintf (attr_path, "%s/%s/dev", block_path, devp->name);
		sysfs_read_attribute_value (attr_path, attr_value, sizeof(attr_value));

		if (!strcmp (attr_value, attr_ref_value)) {
			sprintf (attr_path, "%s/%s", block_path, devp->name);
			sysfs_get_name_from_path (attr_path, devname, FILE_NAME_SIZE);
			break;
		}
	}

	sysfs_close_directory (sdir);
	
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

	for (i = 0; i < VECTOR_SIZE(conf->blist); i++) {
		p = VECTOR_SLOT(conf->blist, i);

		if (memcmp(dev, p, strlen(p)) == 0)
			return 1;
	}
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
	curpath->tur = do_tur(curpath->dev_t);
	curpath->claimed = get_claimed(curpath->dev_t);
	
	/*
	 * get path prio
	 */
	sprintf (buff, "%s /block/%s", conf->default_getprio, curpath->dev);

	dbg("get prio callout :");
	dbg("==================");

	if (execute_program(buff, prio, 16) == 0)
		curpath->priority = atoi(prio);
	else {
		dbg("error calling out %s", buff);
		curpath->priority = -1;
	}
	dbg("devinfo found prio : %u", curpath->priority);

	/*
	 * get path uid
	 */
	for (i = 0; i < VECTOR_SIZE(conf->hwtable); i++) {
		hwe = VECTOR_SLOT(conf->hwtable, i);

		if (MATCH (curpath->vendor_id, hwe->vendor) &&
		    MATCH (curpath->product_id, hwe->product)) {
			/*
			 * callout program
			 */
			dbg("get uid callout :");
			dbg("=================");
			sprintf (buff, "%s /block/%s",
				 hwe->getuid, curpath->dev);

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
	sprintf (buff, "%s /block/%s", conf->default_getuid, curpath->dev);

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
		fprintf(stderr, "[device] feature needs sysfs\n");
		exit_tool(1);
	}
	
	/*
	 * if called from /etc/dev.d , only consider the paths that relate
	 * to the device pointed by conf->dev
	 */
	if (conf->dev != NULL && filepresent(conf->dev)) {
		curpath = zalloc(sizeof (struct path));
		basename(conf->dev, curpath->dev);

		if (devinfo(curpath))
			return 1;

		memcpy(refwwid, curpath->wwid, WWID_SIZE);
		free(curpath);
	}

	/*
	 * if major/minor specified on the cmd line,
	 * only consider affiliated paths
	 */
	if (conf->major >= 0 && conf->minor >= 0) {
		if (devt2devname(buff, conf->major, conf->minor))
			return 1;
		
		curpath = zalloc(sizeof (struct path));
		sprintf(curpath->dev, "%s", buff);

		if (devinfo(curpath))
			return 1;

		memcpy(refwwid, curpath->wwid, WWID_SIZE);
		free(curpath);
	}
		
	sprintf(path, "%s/block", sysfs_path);
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
		sprintf(curpath->dev, "%s", buff);

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

static vector
sort_pathvec_by_prio (vector pathvec)
{
	vector sorted_pathvec;
	struct path * pp;
	int i;
	unsigned int highest_prio;
	unsigned int lowest_prio;
	unsigned int prio;

	if (VECTOR_SIZE(pathvec) < 2)
		return pathvec;

	/*
	 * find out highest & lowest prio
	 */
	pp = VECTOR_SLOT(pathvec, 0);
	highest_prio = pp->priority;
	lowest_prio = pp->priority;

	for (i = 1; i < VECTOR_SIZE(pathvec); i++) {
		pp = VECTOR_SLOT(pathvec, i);

		if (pp->priority > highest_prio)
			highest_prio = pp->priority;

		if (pp->priority < lowest_prio)
			lowest_prio = pp->priority;
	}

	if (lowest_prio == highest_prio)
		return pathvec;

	/* 
	 * sort
	 */
	sorted_pathvec = vector_alloc();
	dbg("sorted_pathvec :");

	for (prio = lowest_prio; prio <= highest_prio; prio++) {
		for (i = 0; i < VECTOR_SIZE(pathvec); i++) {
			pp = VECTOR_SLOT(pathvec, i);

			if (pp->priority == prio) {
				vector_alloc_slot(sorted_pathvec);
				vector_set_slot(sorted_pathvec, pp);
				dbg("%s (%u)", pp->dev, pp->priority);
			}
		}
	}

	return sorted_pathvec;

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

	if (pp->tur)
		printf ("[ready ] ");
	else
		printf ("[faulty] ");

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

	for (k = 0; k < VECTOR_SIZE(pathvec); k++) {
		pp = VECTOR_SLOT(pathvec, k);
		
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

	for (k = 0; k < VECTOR_SIZE(mp); k++) {
		mpp = VECTOR_SLOT(mp, k);

		if (mpp->alias)
			printf ("%s (%s)", mpp->alias, mpp->wwid);
		else
			printf ("%s", mpp->wwid);

		pp = VECTOR_SLOT(mpp->paths, 0);
		printf (" [%.16s]\n", pp->product_id);

		for (i = 0; i < VECTOR_SIZE(mpp->paths); i++) {
			pp = VECTOR_SLOT(mpp->paths, i);
			print_path (pp, SHORT);
		}
	}
}

static struct mpentry *
find_mp (char * wwid)
{
	int i;
	struct mpentry * mpe;

	for (i = 0; i < VECTOR_SIZE(conf->mptable); i++) {
                mpe = VECTOR_SLOT(conf->mptable, i);

                if (strcmp(mpe->wwid, wwid) == 0)
			return mpe;
	}
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

	for (k = 0; k < VECTOR_SIZE(pathvec); k++) {
		pp1 = VECTOR_SLOT(pathvec, k);
		/* skip this path for some reason */

		/* 1. if path has no unique id */
		if (memcmp (empty_buff, pp1->wwid, WWID_SIZE) == 0)
			continue;

		/* 2. if mp with this uid already instanciated */
		for (i = 0; i < VECTOR_SIZE(mp); i++) {
			mpp = VECTOR_SLOT(mp, i);
			if (0 == strcmp (mpp->wwid, pp1->wwid))
				already_done = 1;
		}
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

		if (mpp->size == 0)
			mpp->size = get_disk_size (pp1->dev);

		for (i = k + 1; i < VECTOR_SIZE(pathvec); i++) {
			pp2 = VECTOR_SLOT(pathvec, i);
			if (0 == strcmp (pp1->wwid, pp2->wwid)) {
				vector_alloc_slot (mpp->paths);
				vector_set_slot (mpp->paths,
						 VECTOR_SLOT(pathvec, i));
			}
		}
		vector_set_slot(mp, mpp);
	}
}

static int
dm_simplecmd (int task, const char *name) {
	int r = 0;
	struct dm_task *dmt;

	if (!(dmt = dm_task_create (task)))
		return 0;

	if (!dm_task_set_name (dmt, name))
		goto out;

	r = dm_task_run (dmt);

	out:
		dm_task_destroy (dmt);
		return r;
}

static int
dm_addmap (int task, const char *name, const char *params, unsigned long size) {
	struct dm_task *dmt;

	if (!(dmt = dm_task_create (task)))
		return 0;

	if (!dm_task_set_name (dmt, name))
		goto addout;

	if (!dm_task_add_target (dmt, 0, size, DM_TARGET, params))
		goto addout;

	if (dm_task_run (dmt))
		return 0;

	addout:
	dm_task_destroy (dmt);
	return 1;
}

/*
 * Transforms the path group vector into a proper device map string
 */
void
assemble_map (struct multipath * mp)
{
	int i, j;
	char * p;
	vector pgpaths;
	char * selector;
	int selector_args;
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
	selector = conf->default_selector;
	selector_args = conf->default_selector_args;

	/* 2) override by controler wide settings */
	for (i = 0; i < VECTOR_SIZE(conf->hwtable); i++) {
		hwe = VECTOR_SLOT(conf->hwtable, i);

		if (strcmp(hwe->vendor, pp->vendor_id) == 0 &&
		    strcmp(hwe->product, pp->product_id) == 0) {
			selector = (hwe->selector) ?
				   hwe->selector : conf->default_selector;
			selector_args = (hwe->selector_args) ?
				   hwe->selector_args : conf->default_selector_args;
		}
	}

	/* 3) override by LUN specific settings */
	for (i = 0; i < VECTOR_SIZE(conf->mptable); i++) {
		mpe = VECTOR_SLOT(conf->mptable, i);

		if (strcmp(mpe->wwid, mp->wwid) == 0) {
			selector = (mpe->selector) ?
				   mpe->selector : conf->default_selector;
			selector_args = (mpe->selector_args) ?
				   mpe->selector_args : conf->default_selector_args;
		}
	}

	p = mp->params;
	p += sprintf(p, " %i", VECTOR_SIZE(mp->pg));

	for (i = 0; i < VECTOR_SIZE(mp->pg); i++) {
		pgpaths = VECTOR_SLOT(mp->pg, i);
		p += sprintf(p, " %s %i %i",
			     selector, VECTOR_SIZE(pgpaths), selector_args);

		for (j = 0; j < VECTOR_SIZE(pgpaths); j++)
			p += sprintf(p, " %s",
				     (char *)VECTOR_SLOT(pgpaths, j));
	}
}

static int
map_present (char * str)
{
        int r = 0;
	struct dm_task *dmt;
        struct dm_names *names;
        unsigned next = 0;

	if (!(dmt = dm_task_create (DM_DEVICE_LIST)))
		return 0;

	if (!dm_task_run (dmt))
		goto out;

	if (!(names = dm_task_get_names (dmt)))
		goto out;

	if (!names->dev) {
		goto out;
	}

	do {
		if (0 == strcmp (names->name, str))
			r = 1;
		next = names->next;
		names = (void *) names + next;
	} while (next);

	out:
	dm_task_destroy (dmt);
	return r;
}

static int
setup_map (vector pathvec, vector mp, int slot)
{
	struct path * pp;
	struct multipath * mpp;
	int i, iopolicy;
	char iopolicy_name[32];
	int fd;
	struct hwentry * hwe = NULL;
	struct mpentry * mpe;
	char * mapname;
	int op;

	mpp = VECTOR_SLOT(mp, slot);
	pp = VECTOR_SLOT(mpp->paths, 0);

	/*
	 * don't bother if devmap size is unknown
	 */
	if (mpp->size <= 0)
		return 0;

	/*
	 * don't bother if a constituant path is claimed
	 * FIXME : claimed detection broken, always unclaimed for now
	 */
	for (i = 0; i < VECTOR_SIZE(mpp->paths); i++) {
		pp = VECTOR_SLOT(mpp->paths, i);

		if (pp->claimed)
			return 0;
	}

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

	/* 1) set internal default */
	iopolicy = FAILOVER;
	get_pgpolicy_name(iopolicy_name, FAILOVER);
	dbg("internal default)\tiopolicy = %s", iopolicy_name);

	/* 2) override by config file default */
	if (conf->default_iopolicy >= 0) {
		iopolicy = conf->default_iopolicy;
		get_pgpolicy_name(iopolicy_name, iopolicy);
		dbg("config file default)\tiopolicy = %s", iopolicy_name);
	}
	
	/* 3) override by controler wide setting */
	for (i = 0; i < VECTOR_SIZE(conf->hwtable); i++) {
		hwe = VECTOR_SLOT(conf->hwtable, i);

		if (MATCH (pp->vendor_id, hwe->vendor) &&
		    MATCH (pp->product_id, hwe->product)) {
			iopolicy = hwe->iopolicy;
			get_pgpolicy_name(iopolicy_name, iopolicy);
			dbg("controler override)\tiopolicy = %s", iopolicy_name);
		}
	}

	/* 4) override by LUN specific setting */
	for (i = 0; i < VECTOR_SIZE(conf->mptable); i++) {
		mpe = VECTOR_SLOT(conf->mptable, i);

		if (strcmp(mpe->wwid, mpp->wwid) == 0) {
			iopolicy = mpe->iopolicy;
			get_pgpolicy_name(iopolicy_name, iopolicy);
			dbg("lun override)\t\tiopolicy = %s", iopolicy_name);
		}
	}

	/* 5) cmd line flag has the last word */
	if (conf->iopolicy_flag >= 0) {
		iopolicy = conf->iopolicy_flag;
		get_pgpolicy_name(iopolicy_name, iopolicy);
		dbg("cmd flag override)\tiopolicy = %s", iopolicy_name);
	}

	/*
	 * layered mpp->paths reordering :
	 *  1) sort paths by priority
	 *  2) apply selected grouping policy to paths
	 */

	/* 1) sort by priority */
	mpp->paths = sort_pathvec_by_prio(mpp->paths);

	/* 2) apply selected grouping policy */
	if (iopolicy == MULTIBUS)
		one_group (mpp);

	if (iopolicy == FAILOVER)
		one_path_per_group (mpp);

	if (iopolicy == GROUP_BY_SERIAL)
		group_by_serial (mpp, slot);

	if (iopolicy == GROUP_BY_PRIO)
		group_by_prio (mpp);

	/*
	 * device mapper creation or updating
	 * here we know we'll have garbage on stderr from
	 * libdevmapper. so shut it down temporarily.
	 */
	assemble_map(mpp);

	mapname = mpp->alias ? mpp->alias : mpp->wwid;
	op = map_present(mpp->wwid) ? DM_DEVICE_RELOAD : DM_DEVICE_CREATE;
	
	fd = dup(2);
	close(2);

	if (op == DM_DEVICE_RELOAD)
		if (!dm_simplecmd(DM_DEVICE_SUSPEND, mapname)) {
			dup2(fd, 2);
			close(fd);
			return 0;
		}

	if (dm_addmap(op, mapname, mpp->params, mpp->size)) {
		dm_simplecmd(DM_DEVICE_REMOVE, mapname);
		dup2(fd, 2);
		close(fd);
		return 0;
	}

	if (op == DM_DEVICE_RELOAD)
		if (!dm_simplecmd(DM_DEVICE_RESUME, mapname)) {
			dup2(fd, 2);
			close(fd);
			return 0;
		}

	dup2(fd, 2);
	close(fd);

	if (conf->verbosity > 0)
		printf ("%s", mapname);
	if (conf->verbosity > 1)
		printf (":0 %lu %s%s",
			mpp->size, DM_TARGET,
			mpp->params);
	if (conf->verbosity > 0)
		printf ("\n");

	return 1;
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
		fprintf (stderr, "cannot signal daemon, pidfile not found\n");
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

#define VECTOR_ADDHWE(a, b, c, d, e) \
	hwe = zalloc (sizeof(struct hwentry)); \
	hwe->vendor = zalloc (9 * sizeof(char)); \
	sprintf (hwe->vendor, b); \
	hwe->product = zalloc (17 * sizeof(char)); \
	sprintf (hwe->product, c); \
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
	fprintf (stderr, "Usage: %s\t[-v level] [-d] [-D major minor] [-S]\n",
		progname);
	fprintf (stderr,
		"\t\t\t[-p failover|multibus|group_by_serial|group_by_prio]\n" \
		"\t\t\t[device]\n" \
		"\n" \
		"\t-v level\t\tverbosty level\n" \
		"\t   0 no output\n" \
		"\t   1 print created devmap names only\n" \
		"\t   2 print all paths and multipaths\n" \
		"\t-d\t\tdry run, do not create or update devmaps\n" \
		"\t-D maj min\tlimit scope to the device's multipath\n" \
		"\t\t\t(major minor device reference)\n"
		"\t-S\t\tinhibit signal sending to multipathd\n"
		"\n" \
		"\t-p policy\tforce all maps to specified policy :\n" \
		"\t   failover\t\t- 1 path per priority group\n" \
		"\t   multibus\t\t- all paths in 1 priority group\n" \
		"\t   group_by_serial\t- 1 priority group per serial\n" \
		"\t   group_by_prio\t\t- 1 priority group per priority lvl\n" \
		"\n" \
		"\tdevice\t\tlimit scope to the device's multipath\n" \
		"\t\t\t(udev-style $DEVNAME reference, eg /dev/sdb)\n" \
		);

	exit_tool(1);
}

int
main (int argc, char *argv[])
{
	vector mp;
	vector pathvec;
	int k;
	int try = 0;
	int arg;
	extern char *optarg;
	extern int optind;

	/*
	 * Don't run in parallel
	 */
	while (filepresent(RUN) && try++ < MAXTRY)
		usleep(100000);

	if (filepresent(RUN)) {
		fprintf(stderr, "waited for to long. exiting\n");
		exit(1);
	}
	
	/*
	 * Our turn
	 */
	if (!open(RUN, O_CREAT)) {
		fprintf(stderr, "can't create runfile\n");
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
	conf->iopolicy_flag = -1;	/* do not override defaults */
	conf->major = -1;
	conf->minor = -1;
	conf->signal = 1;		/* 1 == Send a signal to multipathd */
	conf->dev = zalloc(sizeof(char) * FILE_NAME_SIZE);
	conf->default_selector = NULL;
	conf->default_selector_args = 0;
	conf->default_iopolicy = -1;
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
				conf->major = atoi(optarg);
				conf->minor = atoi(argv[optind++]);
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
	if (optind<argc)
		strncpy (conf->dev, argv[optind], FILE_NAME_SIZE);

	/*
	 * read the config file
	 */
	if (filepresent (CONFIGFILE))
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
		exit_tool(1);
	}

	if (get_pathvec_sysfs(pathvec))
		exit_tool(1);

	if (VECTOR_SIZE(pathvec) == 0 && conf->verbosity > 0) {
		fprintf (stdout, "no path found\n");
		exit_tool(0);
	}

	coalesce_paths(mp, pathvec);

	if (conf->verbosity > 1) {
		print_all_path(pathvec);
		print_all_mp(mp);
	}

	/*
	 * last chance to quit before messing with devmaps
	 */
	if (conf->dry_run)
		exit_tool(0);

	if (conf->verbosity > 1)
		fprintf (stdout, "#\n# device maps :\n#\n");

	for (k = 0; k < VECTOR_SIZE(mp); k++)
		setup_map (pathvec, mp, k);

	/*
	 * signal multipathd that new devmaps may have come up
	 */
	if (conf->signal)
		signal_daemon ();
	
	/*
	 * free allocs
	 */
	free (mp);
	free (pathvec);

	/*
	 * release runfile
	 */
	unlink (RUN);

	exit (0);
}
