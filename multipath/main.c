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
#include "global.h"
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
sysfsdevice2devname (char *devname, char *device)
{
	char sysfs_path[FILE_NAME_SIZE];
	char block_path[FILE_NAME_SIZE];
	char link_path[FILE_NAME_SIZE];
	int r;

	if (sysfs_get_mnt_path (sysfs_path, FILE_NAME_SIZE)) {
		fprintf (stderr, "[device] feature needs sysfs\n");
		exit (1);
	}
	
	sprintf (link_path, "%s%s/block", sysfs_path, device);
	
	r = sysfs_get_link (link_path, block_path, FILE_NAME_SIZE);

	if (r != 0)
		return 1;

	sysfs_get_name_from_path (block_path, devname, FILE_NAME_SIZE);

	return 0;
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

	sysfs_devinfo(curpath);
	get_serial(curpath->serial, curpath->dev_t);
	curpath->tur = do_tur(curpath->dev_t);
	curpath->claimed = get_claimed(curpath->dev_t);
	
	/*
	 * get path uid
	 */
	for (i = 0; i < VECTOR_SIZE(conf->hwtable); i++) {
		hwe = VECTOR_SLOT(conf->hwtable, i);
		sprintf (buff, "%s /block/%s", hwe->getuid, curpath->dev);

		if (MATCH (curpath->vendor_id, hwe->vendor) &&
		    MATCH (curpath->product_id, hwe->product)) {
			/*
			 * callout program
			 */
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
	sprintf (buff, "%s /block/%s", conf->default_getuid, curpath->dev);

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

	/*
	 * if called from hotplug, only consider the paths that relate
	 * to the device pointed by conf.hotplugdev
	 */
	if (strncmp ("/devices", conf->hotplugdev, 8) == 0) {
		if (sysfsdevice2devname (buff, conf->hotplugdev))
			return 1;

		curpath = zalloc (sizeof (struct path));
		sprintf (curpath->dev, "%s", buff);

		if (devinfo (curpath))
			return 1;

		memcpy (refwwid, curpath->wwid, WWID_SIZE);
		free (curpath);
	}

	/*
	 * if major/minor specified on the cmd line,
	 * only consider affiliated paths
	 */
	if (conf->major >= 0 && conf->minor >= 0) {
		if (devt2devname (buff, conf->major, conf->minor))
			return 1;
		
		curpath = zalloc (sizeof (struct path));
		sprintf (curpath->dev, "%s", buff);

		if (devinfo (curpath))
			return 1;

		memcpy (refwwid, curpath->wwid, WWID_SIZE);
		free (curpath);
	}
		
	if (sysfs_get_mnt_path (sysfs_path, FILE_NAME_SIZE)) {
		fprintf (stderr, "[device] feature needs sysfs\n");
		exit_tool(1);
	}
	
	sprintf (path, "%s/block", sysfs_path);
	sdir = sysfs_open_directory (path);
	sysfs_read_directory (sdir);

	dlist_for_each_data (sdir->subdirs, devp, struct sysfs_directory) {
		if (blacklist (devp->name))
			continue;

		sysfs_read_directory (devp);

		if (devp->links == NULL)
			continue;

		dlist_for_each_data (devp->links, linkp, struct sysfs_link) {
			if (!strncmp (linkp->name, "device", 6))
				break;
		}

		if (linkp == NULL) {
			continue;
		}

		basename (devp->path, buff);
		curpath = zalloc (sizeof (struct path));
		sprintf (curpath->dev, "%s", buff);

		if (devinfo (curpath)) {
			free (curpath);
			continue;
		}

		if (memcmp (empty_buff, refwwid, WWID_SIZE) != 0 && 
		    memcmp (curpath->wwid, refwwid, WWID_SIZE) != 0) {
			free (curpath);
			continue;
		}

		basename (linkp->target, buff);
		sscanf (buff, "%i:%i:%i:%i",
			&curpath->sg_id.host_no,
			&curpath->sg_id.channel,
			&curpath->sg_id.scsi_id,
			&curpath->sg_id.lun);

		vector_alloc_slot (pathvec);
		vector_set_slot (pathvec, curpath);
	}
	sysfs_close_directory (sdir);
	return 0;
}

static void
sort_pathvec_by_prio (vector pathvec)
{
	vector sorted_pathvec;
	struct path * pp;
	int i;
	int highest_prio;
	int lowest_prio;
	int prio;

	if (VECTOR_SIZE(pathvec) < 2)
		return;

	sorted_pathvec = vector_alloc();

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

	/* 
	 * sort
	 */
	for (prio = highest_prio; prio >= lowest_prio; prio--) {
		for (i = 0; i < VECTOR_SIZE(pathvec); i++) {
			pp = VECTOR_SLOT(pathvec, i);

			if (pp->priority == prio) {
				vector_alloc_slot(sorted_pathvec);
				vector_set_slot(sorted_pathvec, pp);
			}
		}
	}

	/*
	 * permute to sorted_pathvec
	 */
	pathvec = sorted_pathvec;
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
		printf ("%s", mpp->wwid);
		pp = VECTOR_SLOT(mpp->paths, 0);
		printf (" [%.16s]\n", pp->product_id);

		for (i = 0; i < VECTOR_SIZE(mpp->paths); i++) {
			pp = VECTOR_SLOT(mpp->paths, i);
			print_path (pp, SHORT);
		}
	}
}

static void
coalesce_paths (vector mp, vector pathvec)
{
	int k, i, already_done;
	char empty_buff[WWID_SIZE];
	struct multipath * mpp;
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

		/* at this point, we know we really got a new mp */
		vector_alloc_slot(mp);
		mpp = zalloc(sizeof(struct multipath));
		strcpy (mpp->wwid, pp1->wwid);
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
dm_addmap (int task, const char *name, const char *params, long size) {
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

static int
setup_map (vector pathvec, vector mp, int slot,  int op)
{
	struct path * pp;
	struct multipath * mpp;
	int i, iopolicy;
	int fd;
	struct hwentry * hwe = NULL;
	struct mpentry * mpe;

	mpp = VECTOR_SLOT(mp, slot);
	pp = VECTOR_SLOT(mpp->paths, 0);

	/*
	 * don't bother if devmap size is unknown
	 */
	if (mpp->size < 0)
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

	/* 1) set internal default */
	iopolicy = FAILOVER;
	dbg("internal default) iopolicy = %i", iopolicy);

	/* 2) override by config file default */
	if (conf->default_iopolicy >= 0) {
		iopolicy = conf->default_iopolicy;
		dbg("config file default) iopolicy = %i", iopolicy);
	}
	
	/* 3) override by controler wide setting */
	for (i = 0; i < VECTOR_SIZE(conf->hwtable); i++) {
		hwe = VECTOR_SLOT(conf->hwtable, i);

		if (MATCH (pp->vendor_id, hwe->vendor) &&
		    MATCH (pp->product_id, hwe->product)) {
			iopolicy = hwe->iopolicy;
			dbg("controler override) iopolicy = %i", iopolicy);
		}
	}

	/* 4) override by LUN specific setting */
	for (i = 0; i < VECTOR_SIZE(conf->mptable); i++) {
		mpe = VECTOR_SLOT(conf->mptable, i);

		if (strcmp(mpe->wwid, mpp->wwid) == 0) {
			iopolicy = mpe->iopolicy;
			dbg("lun override) iopolicy = %i", iopolicy);
		}
	}

	/* 5) cmd line flag has the last word */
	if (conf->iopolicy_flag >= 0) {
		iopolicy = conf->iopolicy_flag;
		dbg("cmd flag override) iopolicy = %i", iopolicy);
	}

	/*
	 * layered mpp->paths reordering :
	 *  1) sort paths by priority
	 *  2) apply selected grouping policy to paths
	 */

	/* 1) sort by priority */
	sort_pathvec_by_prio(mpp->paths);

	/* 2) apply selected grouping policy */
	if (iopolicy == MULTIBUS)
		one_group (mpp);

	if (iopolicy == FAILOVER)
		one_path_per_group (mpp);

	if (iopolicy == GROUP_BY_SERIAL)
		group_by_serial (mpp, slot);

	/*
	 * device mapper creation or updating
	 * here we know we'll have garbage on stderr from
	 * libdevmapper. so shut it down temporarily.
	 */
	assemble_map(mpp);

	fd = dup(2);
	close(2);

	if (op == DM_DEVICE_RELOAD)
		if (!dm_simplecmd(DM_DEVICE_SUSPEND, mpp->wwid))
			return 0;

	if (dm_addmap(op, mpp->wwid, mpp->params, mpp->size)) {
		dm_simplecmd(DM_DEVICE_REMOVE, mpp->wwid);
		return 0;
	}

	if (op == DM_DEVICE_RELOAD)
		if (!dm_simplecmd(DM_DEVICE_RESUME, mpp->wwid))
			return 0;

	dup2(fd, 2);
	close(fd);

	if (!conf->quiet) {

		if (op == DM_DEVICE_RELOAD)
			printf ("U:");

		if (op == DM_DEVICE_CREATE)
			printf ("N:");

		printf ("%s:0 %li %s%s\n",
			mpp->wwid,
			mpp->size, DM_TARGET,
			mpp->params);
	}

	return 1;
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

static int
filepresent (char * run) {
	struct stat buf;

	if(!stat (run, &buf))
		return 1;
	return 0;
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
	fprintf (stderr, "Usage: %s\t[-v|-q] [-d] [-D major minor] [-S]\n",
		progname);
	fprintf (stderr,
		"\t\t\t[-p failover|multibus|group_by_serial]\n" \
		"\t\t\t[device]\n" \
		"\n" \
		"\t-v\t\tverbose, print all paths and multipaths\n" \
		"\t-q\t\tquiet, no output at all\n" \
		"\t-d\t\tdry run, do not create or update devmaps\n" \
		"\t-D maj min\tlimit scope to the device's multipath\n" \
		"\t\t\t(major minor device reference)\n"
		"\t-S\t\tinhibit signal sending to multipathd\n"
		"\n" \
		"\t-p policy\tforce all maps to specified policy :\n" \
		"\t   failover\t\t- 1 path per priority group\n" \
		"\t   multibus\t\t- all paths in 1 priority group\n" \
		"\t   group_by_serial\t- 1 priority group per serial\n" \
		"\t   group_by_tur\t\t- 1 priority group per TUR state\n" \
		"\n" \
		"\tdevice\t\tlimit scope to the device's multipath\n" \
		"\t\t\t(hotplug-style $DEVPATH reference)\n" \
		);

	exit_tool(1);
}

int
main (int argc, char *argv[])
{
	vector mp;
	struct multipath * mpp;
	vector pathvec;
	int k;
	int try = 0;
	int arg;
	extern char *optarg;
	extern int optind;

	/* Don't run in parallel */
	while (filepresent (RUN) && try++ < MAXTRY)
		usleep (100000);

	if (filepresent (RUN)) {
		fprintf (stderr, "waited for to long. exiting\n");
		exit (1);
	}
	
	/* Our turn */
	if (!open (RUN, O_CREAT)) {
		fprintf (stderr, "can't create runfile\n");
		exit (1);
	}
		
	/* alloc config struct */
	conf = malloc(sizeof(struct config));
				
	/* internal defaults */
	conf->dry_run = 0;		/* 1 == Do not Create/Update devmaps */
	conf->verbose = 0;		/* 1 == Print pathvec and mp */
	conf->quiet = 0;		/* 1 == Do not even print devmaps */
	conf->iopolicy_flag = -1;	/* do not override defaults */
	conf->major = -1;
	conf->minor = -1;
	conf->signal = 1;		/* 1 == Send a signal to multipathd */
	conf->hotplugdev = zalloc(sizeof(char) * FILE_NAME_SIZE);
	conf->default_selector = NULL;
	conf->default_selector_args = 0;
	conf->default_iopolicy = -1;
	conf->mptable = NULL;
	conf->hwtable = NULL;
	conf->blist = NULL;

	while ((arg = getopt(argc, argv, ":vqdSi:p:D:")) != EOF ) {
		switch(arg) {
			case 1: printf("optarg : %s\n",optarg);
				break;
			case 'v':
				if (conf->quiet == 1)
					usage (argv[0]);
				conf->verbose = 1;
				break;
			case 'q':                
				if (conf->verbose == 1)
					usage (argv[0]);
				conf->quiet = 1;
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
		strncpy (conf->hotplugdev, argv[optind], FILE_NAME_SIZE);

	/* read the config file */
	if (filepresent (CONFIGFILE))
		init_data (CONFIGFILE, init_keywords);
	
	/* fill the voids left in the config file */
	if (conf->hwtable == NULL) {
		conf->hwtable = vector_alloc();
		setup_default_hwtable(conf->hwtable);
	}
	
	if (conf->blist == NULL) {
		conf->blist = vector_alloc();
		setup_default_blist(conf->blist);
	}

	if (conf->mptable == NULL) {
		conf->mptable = vector_alloc();
	}

	if (conf->default_selector == NULL) {
		conf->default_selector = malloc(sizeof(char) * 12);
		sprintf (conf->default_selector, "round-robin");
	}

	if (conf->udev_dir == NULL) {
		conf->udev_dir = malloc(sizeof(char) * 6);
		sprintf (conf->udev_dir, "/udev");
	}

	dbg("conf->udev_dir = %s", conf->udev_dir);
	/* allocate the two core vectors to store paths and multipaths*/
	mp = vector_alloc();
	pathvec = vector_alloc();

	if (mp == NULL || pathvec == NULL) {
		fprintf(stderr, "can not allocate memory\n");
		exit_tool(1);
	}

	if (get_pathvec_sysfs(pathvec))
		exit_tool(1);

	if (VECTOR_SIZE(pathvec) == 0) {
		fprintf (stdout, "no path found\n");
		exit_tool(0);
	}

	coalesce_paths(mp, pathvec);

	if (conf->verbose) {
		print_all_path(pathvec);
		print_all_mp(mp);
	}

	/* last chance to quit before messing with devmaps */
	if (conf->dry_run)
		exit_tool(0);

	if (conf->verbose)
		fprintf (stdout, "#\n# device maps :\n#\n");

	for (k = 0; k < VECTOR_SIZE(mp); k++) {
		mpp = VECTOR_SLOT (mp, k);
		if (map_present (mpp->wwid)) {
			setup_map (pathvec, mp, k, DM_DEVICE_RELOAD);
		} else {
			setup_map (pathvec, mp, k, DM_DEVICE_CREATE);
		}
	}

	/* signal multipathd that new devmaps may have come up */
	if (conf->signal)
		signal_daemon ();
	
	/* free allocs */
	free (mp);
	free (pathvec);

	/* release runfile */
	unlink (RUN);

	exit (0);
}
