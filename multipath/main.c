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
#include <linux/kdev_t.h>
#include <string.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <dlist.h>
#include <sysfs/libsysfs.h>

#include "../libdevmapper/libdevmapper.h"
#include "main.h"
#include "devinfo.h"
#include "hwtable.h"
#include "pgpolicies.h"
#include "parser.h"
#include "dict.h"
#include "vector.h"
#include "memory.h"

/* helpers */
#define argis(x) if (0 == strcmp (x, argv[i]))
#define MATCH(x,y) 0 == strncmp (x, y, sizeof (y))

static int
sysfsdevice2devname (char *devname, char *device)
{
	char sysfs_path[FILE_NAME_SIZE];
	char block_path[FILE_NAME_SIZE];
	char link_path[FILE_NAME_SIZE];
	int r;

	if (sysfs_get_mnt_path (sysfs_path, FILE_NAME_SIZE)) {
		fprintf (stderr, "[device] feature available with sysfs only\n");
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

	for (i = 0; i < VECTOR_SIZE(blist); i++) {
		p = VECTOR_SLOT(blist, i);

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

	get_lun_strings (curpath->vendor_id,
			curpath->product_id,
			curpath->rev,
			curpath->sg_dev);
	
	get_serial (curpath->serial, curpath->sg_dev);
	curpath->tur = do_tur (curpath->sg_dev);

	for (i = 0; i < VECTOR_SIZE(hwtable); i++) {
		hwe = VECTOR_SLOT(hwtable, i);

		if (MATCH (curpath->vendor_id, hwe->vendor) &&
		    MATCH (curpath->product_id, hwe->product)) {
			curpath->iopolicy = hwe->iopolicy;

			if (hwe->getuid (curpath->sg_dev, curpath->wwid))
				return 1;
		}
	}

	return 0;
}

static int
get_pathvec_sysfs (struct env * conf, vector pathvec)
{
	struct sysfs_directory * sdir;
	struct sysfs_directory * devp;
	struct sysfs_link * linkp;
	char refwwid[WWID_SIZE];
	char empty_buff[WWID_SIZE];
	char buff[FILE_NAME_SIZE];
	char path[FILE_NAME_SIZE];
	struct path * curpath;

	memset (empty_buff, 0, WWID_SIZE);
	memset (refwwid, 0, WWID_SIZE);

	/* if called from hotplug, only consider the paths that relate
	   to the device pointed by conf.hotplugdev */

	if (strncmp ("/devices", conf->hotplugdev, 8) == 0) {
		if (sysfsdevice2devname (buff, conf->hotplugdev))
			return 0;

		curpath = zalloc (sizeof (struct path));
		sprintf (curpath->sg_dev, "/dev/%s", buff);

		if (devinfo (curpath))
			return 0;

		memcpy (refwwid, curpath->wwid, WWID_SIZE);
		free (curpath);
	}

	/* if major/minor specified on the cmd line,
	   only consider affiliated paths */

	if (conf->major >= 0 && conf->minor >= 0) {
		if (devt2devname (buff, conf->major, conf->minor))
			return 0;
		
		curpath = zalloc (sizeof (struct path));
		sprintf (curpath->sg_dev, "/dev/%s", buff);

		if (devinfo (curpath))
			return 0;

		memcpy (refwwid, curpath->wwid, WWID_SIZE);
		free (curpath);
	}
		

	sprintf (path, "%s/block", conf->sysfs_path);
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
		sprintf (curpath->sg_dev, "/dev/%s", buff);
		sprintf (curpath->dev, "/dev/%s", buff);

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

/* print_path style */
#define ALL	0
#define NOWWID	1

static void
print_path (struct path * pp, int style)
{
	if (style != NOWWID)
		printf ("%s ", pp->wwid);
	else
		printf (" \\_");

	printf ("(%i %i %i %i) ",
	       pp->sg_id.host_no,
	       pp->sg_id.channel,
	       pp->sg_id.scsi_id,
	       pp->sg_id.lun);

	/* for 2.4 kernels, sg_dev should be printed */
	if (0 != strcmp (pp->sg_dev, pp->dev))
		printf ("%s ", pp->sg_dev);

	printf ("%s ", pp->dev);
	printf ("[%.16s]\n", pp->product_id);
}

static void
print_all_path (struct env * conf, vector pathvec)
{
	int k;
	char empty_buff[WWID_SIZE];
	struct path * pp;

	/* initialize a cmp 0-filled buffer */
	memset (empty_buff, 0, WWID_SIZE);

	fprintf (stdout, "# all paths :\n");

	for (k = 0; k < VECTOR_SIZE(pathvec); k++) {
		pp = VECTOR_SLOT(pathvec, k);
		
		/* leave out paths with incomplete devinfo */
		if (memcmp (empty_buff, pp->wwid, WWID_SIZE) == 0)
			continue;

		print_path (pp, ALL);
	}
}

static void
print_all_mp (vector pathvec, vector mp)
{
	int k, i;
	struct multipath * mpp;
	struct path * pp = NULL;

	fprintf (stdout, "# all multipaths :\n");

	for (k = 0; k < VECTOR_SIZE(mp); k++) {
		mpp = VECTOR_SLOT(mp, k);
		printf ("%s\n", mpp->wwid);

		for (i = 0; i <= mpp->npaths; i++) {
			pp = VECTOR_SLOT(pathvec, mpp->pindex[i]);
			print_path (pp, NOWWID);
		}
	}
}

static void
coalesce_paths (struct env * conf, vector mp, vector pathvec)
{
	int k, i, np, already_done;
	char empty_buff[WWID_SIZE];
	struct multipath * mpp;
	struct path * pp1;
	struct path * pp2;

	already_done = 0;
	memset (empty_buff, 0, WWID_SIZE);

	for (k = 0; k < VECTOR_SIZE(pathvec) - 1; k++) {
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
		np = 0;
		vector_alloc_slot(mp);
		mpp = malloc(sizeof(struct multipath));
		strcpy (mpp->wwid, pp1->wwid);
		mpp->pindex[np] = k;

		if (mpp->size == 0)
			mpp->size = get_disk_size (pp1->dev);

		for (i = k + 1; i < VECTOR_SIZE(pathvec); i++) {
			pp2 = VECTOR_SLOT(pathvec, i);
			if (0 == strcmp (pp1->wwid, pp2->wwid)) {
				np++;
				mpp->pindex[np] = i;
				mpp->npaths = np;
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

	if (!dm_task_run (dmt))
		goto addout;

	addout:
	dm_task_destroy (dmt);
	return 1;
}

/* helper for grouping policy selection */

#define policyis(x) params_p == &params[0] && \
		    (conf->iopolicy == x || \
		    (pp->iopolicy == x && conf->iopolicy == -1))

static int
setup_map (struct env * conf, vector pathvec, struct multipath * mpp, int op)
{
	char params[255];
	char * params_p;
	struct path * pp;

	pp = VECTOR_SLOT(pathvec, mpp->pindex[0]);
	params_p = &params[0];

	/* paths grouping policy selector */
	/* implementations in pgpolicies.c */

	if (policyis (MULTIBUS))
		one_group (mpp, pathvec, params_p);

	if (policyis (FAILOVER))
		one_path_per_group (mpp, pathvec, params_p);

	if (policyis (GROUP_BY_SERIAL))
		group_by_serial (mpp, pathvec, params_p);

	if (policyis (GROUP_BY_TUR))
		group_by_tur (mpp, pathvec, params_p);

	if (mpp->size < 0)
		return 0;

	if (!conf->quiet) {

		if (op == DM_DEVICE_RELOAD)
			printf ("U:");

		if (op == DM_DEVICE_CREATE)
			printf ("N:");

		printf ("%s:0 %li %s%s\n",
			mpp->wwid, mpp->size, DM_TARGET, params);
	}

	if (op == DM_DEVICE_RELOAD)
		dm_simplecmd (DM_DEVICE_SUSPEND, mpp->wwid);

	dm_addmap (op, mpp->wwid, params, mpp->size);

	if (op == DM_DEVICE_RELOAD)
		dm_simplecmd (DM_DEVICE_RESUME, mpp->wwid);

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

	unlink (RUN);
	exit (1);
}

int
main (int argc, char *argv[])
{
	vector mp;
	struct multipath * mpp;
	vector pathvec;
	struct env conf;
	int i, k;
	int try = 0;

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
		
	/* Default behaviour */
	conf.dry_run = 0;	/* 1 == Do not Create/Update devmaps */
	conf.verbose = 0;	/* 1 == Print pathvec and mp */
	conf.quiet = 0;		/* 1 == Do not even print devmaps */
	conf.iopolicy = -1;	/* do not override defaults */
	conf.major = -1;
	conf.minor = -1;
	conf.signal = 1;	/* 1 == Do send a signal to multipathd */

	/* argv parser */
	for (i = 1; i < argc; ++i) {

		argis ("-v") {
			if (conf.quiet == 1)
				usage (argv[0]);
			conf.verbose = 1;
		}
		
		else argis ("-D") {
			conf.major = atoi (argv[++i]);
			conf.minor = atoi (argv[++i]);
		}
		
		else argis ("-q") {
			if (conf.verbose == 1)
				usage (argv[0]);
			conf.quiet = 1;
		}
		
		else argis ("-d") {
			conf.dry_run = 1;
			conf.signal = 0;
		}
		
		else argis ("-S")
			conf.signal = 0;
		
		else argis ("-p") {
			i++;
			argis ("failover")
				conf.iopolicy = FAILOVER;
			
			argis ("multibus")
				conf.iopolicy = MULTIBUS;

			argis ("group_by_serial")
				conf.iopolicy = GROUP_BY_SERIAL;

			argis ("group_by_tur")
				conf.iopolicy = GROUP_BY_TUR;
		}
		
		else if (*argv[i] == '-') {
			fprintf (stderr, "Unknown switch: %s\n", argv[i]);
			usage (argv[0]);
		} else
			strncpy (conf.hotplugdev, argv[i], FILE_NAME_SIZE);
	}

	init_data (CONFIGFILE, init_keywords);

	/* dynamic allocations */
	mp = vector_alloc();
	pathvec = vector_alloc();

	if (mp == NULL || pathvec == NULL) {
		fprintf (stderr, "can not allocate memory\n");
		unlink (RUN);
		exit (1);
	}

	if (sysfs_get_mnt_path (conf.sysfs_path, FILE_NAME_SIZE))
		exit (1);

	get_pathvec_sysfs (&conf, pathvec);
	coalesce_paths (&conf, mp, pathvec);

	if (conf.verbose) {
		print_all_path (&conf, pathvec);
		print_all_mp (pathvec, mp);
	}

	/* last chance to quit before messing with devmaps */
	if (conf.dry_run) {
		unlink (RUN);
		exit (0);
	}

	if (conf.verbose)
		fprintf (stdout, "# device maps :\n");

	for (k = 0; k < VECTOR_SIZE(mp); k++) {
		mpp = VECTOR_SLOT(mp, k);
		if (map_present (mpp->wwid)) {
			setup_map (&conf, pathvec, mpp, DM_DEVICE_RELOAD);
		} else {
			setup_map (&conf, pathvec, mpp, DM_DEVICE_CREATE);
		}
	}

	/* signal multipathd that new devmaps may have come up */
	if (conf.signal)
		signal_daemon ();
	
	/* free allocs */
	free (mp);
	free (pathvec);

	/* release runfile */
	unlink (RUN);

	exit (0);
}
