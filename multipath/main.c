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
#include "configfile.h"
#include "hwtable.h"
#include "pgpolicies.h"

/* argv parser helper */
#define argis(x) if (0 == strcmp (x, argv[i]))

/* not nice */
void *getuid_list[] = {
	&get_null_uid,	/* returns 0x0 */
	&get_evpd_wwid,	/* returns the LU WWID stored in EVPD page 0x83 */
	NULL,		/* array terminator */
};
        
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
		sysfs_read_attribute_value (attr_path, attr_value, 16);

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
	static struct {
		char * headstr;
		int lengh;
	} blist[] = {
		{"cciss", 5},
		{"fd", 2},
		{"hd", 2},
		{"md", 2},
		{"dm", 2},
		{"sr", 2},
		{"scd", 3},
		{"ram", 3},
		{"raw", 3},
		{"loop", 4},
		{NULL, 0},
	};

	for (i = 0; blist[i].lengh; i++) {
		if (strncmp (dev, blist[i].headstr, blist[i].lengh) == 0)
			return 1;
	}
	return 0;
}

static int
devinfo (struct path *curpath, struct hwentry * hwtable)
{
	int i;

	get_lun_strings (curpath->vendor_id,
			curpath->product_id,
			curpath->rev,
			curpath->sg_dev);
	
	get_serial (curpath->serial, curpath->sg_dev);
	curpath->tur = do_tur (curpath->sg_dev);

	for (i = 0; hwtable[i].getuid; i++) {

		if (strncmp (curpath->vendor_id, hwtable[i].vendor, 8) == 0 &&
		    strncmp (curpath->product_id, hwtable[i].product, 16) == 0) {
			
			curpath->iopolicy = hwtable[i].iopolicy;

			if (getuid == NULL)
				return 1;

			if (hwtable[i].getuid (curpath->sg_dev, curpath->wwid))
				return 1;
		}
	}

	return 0;
}

static int
get_all_paths_sysfs (struct env * conf, struct path * all_paths)
{
	int k=0;
	struct sysfs_directory * sdir;
	struct sysfs_directory * devp;
	struct sysfs_link * linkp;
	char refwwid[WWID_SIZE];
	char empty_buff[WWID_SIZE];
	char buff[FILE_NAME_SIZE];
	char path[FILE_NAME_SIZE];
	struct path curpath;

	memset (empty_buff, 0, WWID_SIZE);
	memset (refwwid, 0, WWID_SIZE);

	/* if called from hotplug, only consider the paths that relate
	   to the device pointed by conf.hotplugdev */

	if (strncmp ("/devices", conf->hotplugdev, 8) == 0) {
		if (sysfsdevice2devname (buff, conf->hotplugdev))
			return 0;

		sprintf (curpath.sg_dev, "/dev/%s", buff);

		if (devinfo (&curpath, conf->hwtable))
			return 0;

		strcpy (refwwid, curpath.wwid);
		memset (&curpath, 0, sizeof (path));
	}

	/* if major/minor specified on the cmd line,
	   only consider affiliated paths */

	if (conf->major >= 0 && conf->minor >= 0) {
		if (devt2devname (buff, conf->major, conf->minor))
			return 0;
		
		sprintf (curpath.sg_dev, "/dev/%s", buff);

		if (devinfo (&curpath, conf->hwtable))
			return 0;

		strcpy (refwwid, curpath.wwid);
		memset (&curpath, 0, sizeof (path));
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
		sprintf (curpath.sg_dev, "/dev/%s", buff);

		if (devinfo (&curpath, conf->hwtable)) {
			memset (&curpath, 0, sizeof (path));
			continue;
		}

		if (memcmp (empty_buff, refwwid, WWID_SIZE) != 0 && 
		    strncmp (curpath.wwid, refwwid, WWID_SIZE) != 0) {
			memset (&curpath, 0, sizeof (path));
			continue;
		}

		strcpy (all_paths[k].sg_dev, curpath.sg_dev);
		strcpy (all_paths[k].dev, curpath.sg_dev);
		strcpy (all_paths[k].wwid, curpath.wwid);
		strcpy (all_paths[k].vendor_id, curpath.vendor_id);
		strcpy (all_paths[k].product_id, curpath.product_id);
		all_paths[k].iopolicy = curpath.iopolicy;
		all_paths[k].tur = curpath.tur;

		/* done with curpath, zero for reuse */
		memset (&curpath, 0, sizeof (path));

		basename (linkp->target, buff);
		sscanf (buff, "%i:%i:%i:%i",
			&all_paths[k].sg_id.host_no,
			&all_paths[k].sg_id.channel,
			&all_paths[k].sg_id.scsi_id,
			&all_paths[k].sg_id.lun);
		k++;
	}
	sysfs_close_directory (sdir);
	return 0;
}

static int
get_all_scsi_ids (struct env * conf, struct scsi_dev * all_scsi_ids)
{
	int k, big, little, res, host_no, fd;
	char buff[64];
	char fname[FILE_NAME_SIZE];
	struct scsi_idlun my_scsi_id;

	for (k = 0; k < conf->max_devs; k++) {
		strcpy (fname, "/dev/sd");
		if (k < 26) {
			buff[0] = 'a' + (char) k;
			buff[1] = '\0';
			strcat (fname, buff);
		} else if (k <= 255) {
			/* assumes sequence goes x,y,z,aa,ab,ac etc */
			big = k / 26;
			little = k - (26 * big);
			big = big - 1;

			buff[0] = 'a' + (char) big;
			buff[1] = 'a' + (char) little;
			buff[2] = '\0';
			strcat (fname, buff);
		} else
			strcat (fname, "xxxx");

		if ((fd = open (fname, O_RDONLY)) < 0) {
			if (conf->verbose)
				fprintf (stderr, "can't open %s. mknod ?",
					fname); 
			continue;
		}

		res = ioctl (fd, SCSI_IOCTL_GET_IDLUN, &my_scsi_id);
		if (res < 0) {
			close (fd);
			printf ("Could not get scsi idlun\n");
			continue;
		}

		res = ioctl (fd, SCSI_IOCTL_GET_BUS_NUMBER, &host_no);
		if (res < 0) {
			close (fd);
			printf ("Could not get host_no\n");
			continue;
		}

		close (fd);

		strcpy (all_scsi_ids[k].dev, fname);
		all_scsi_ids[k].scsi_id = my_scsi_id;
		all_scsi_ids[k].host_no = host_no;
	}
	return 0;
}

static int
get_all_paths_nosysfs (struct env * conf, struct path * all_paths)
{
	int k, i, fd;
	char buff[FILE_NAME_SIZE];
	char file_name[FILE_NAME_SIZE];
	struct scsi_dev * all_scsi_ids;

	/* get all scsi ids for pivoting sd / sg */
	all_scsi_ids = malloc (conf->max_devs * sizeof (struct scsi_dev));

	if (all_scsi_ids == NULL) {
		fprintf (stderr, "can not allocate memory\n");
		unlink (RUN);
		exit (1);
	}

	get_all_scsi_ids (conf, all_scsi_ids);

	for (k = 0; k < conf->max_devs; k++) {
		strcpy (file_name, "/dev/sg");
		sprintf (buff, "%d", k);
		strncat (file_name, buff, FILE_NAME_SIZE);
		strcpy (all_paths[k].sg_dev, file_name);

		devinfo (&all_paths[k], conf->hwtable);

		if ((fd = open (all_paths[k].sg_dev, O_RDONLY)) < 0)
			return 0;

		if (0 > ioctl (fd, SG_GET_SCSI_ID, &(all_paths[k].sg_id)))
			printf ("device %s failed on sg ioctl, skip\n",
			       file_name);

		close (fd);

		for (i = 0; i < conf->max_devs; i++) {
			if ((all_paths[k].sg_id.host_no ==
			     all_scsi_ids[i].host_no)
			    && (all_paths[k].sg_id.scsi_id ==
				(all_scsi_ids[i].scsi_id.dev_id & 0xff))
			    && (all_paths[k].sg_id.lun ==
				((all_scsi_ids[i].scsi_id.dev_id >> 8) & 0xff))
			    && (all_paths[k].sg_id.channel ==
				((all_scsi_ids[i].scsi_id.
				  dev_id >> 16) & 0xff))) {
				strcpy (all_paths[k].dev, all_scsi_ids[i].dev);
				break;
			}
		}
	}

	free (all_scsi_ids);
	return 0;
}

/* print_path style */
#define ALL	0
#define NOWWID	1

static void
print_path (struct path * all_paths, int k, int style)
{
	if (style != NOWWID)
		printf ("%s ", all_paths[k].wwid);
	else
		printf (" \\_");

	printf ("(%i %i %i %i) ",
	       all_paths[k].sg_id.host_no,
	       all_paths[k].sg_id.channel,
	       all_paths[k].sg_id.scsi_id,
	       all_paths[k].sg_id.lun);

	/* for 2.4 kernels, sg_dev should be printed */
	if (0 != strcmp (all_paths[k].sg_dev, all_paths[k].dev))
		printf ("%s ", all_paths[k].sg_dev);

	printf ("%s ", all_paths[k].dev);
	printf ("[%.16s]\n", all_paths[k].product_id);
}

static void
print_all_path (struct env * conf, struct path * all_paths)
{
	int k;
	char empty_buff[WWID_SIZE];

	/* initialize a cmp 0-filled buffer */
	memset (empty_buff, 0, WWID_SIZE);

	fprintf (stdout, "# all paths :\n");

	for (k = 0; k < conf->max_devs; k++) {
		
		/* leave out paths with incomplete devinfo */
		if (memcmp (empty_buff, all_paths[k].wwid, WWID_SIZE) == 0)
			continue;

		print_path (all_paths, k, ALL);
	}
}

static void
print_all_mp (struct path * all_paths, struct multipath * mp, int nmp)
{
	int k, i;

	fprintf (stdout, "# all multipaths :\n");

	for (k = 0; k <= nmp; k++) {
		printf ("%s\n", mp[k].wwid);

		for (i = 0; i <= mp[k].npaths; i++)
			print_path (all_paths, PINDEX(k,i), NOWWID);
	}
}

static int
coalesce_paths (struct env * conf, struct multipath * mp,
	       struct path * all_paths)
{
	int k, i, nmp, np, already_done;
	char empty_buff[WWID_SIZE];

	nmp = -1;
	already_done = 0;
	memset (empty_buff, 0, WWID_SIZE);

	for (k = 0; k < conf->max_devs - 1; k++) {
		/* skip this path for some reason */

		/* 1. if path has no unique id */
		if (memcmp (empty_buff, all_paths[k].wwid, WWID_SIZE) == 0)
			continue;

		/* 2. if mp with this uid already instanciated */
		for (i = 0; i <= nmp; i++) {
			if (0 == strcmp (mp[i].wwid, all_paths[k].wwid))
				already_done = 1;
		}
		if (already_done) {
			already_done = 0;
			continue;
		}

		/* at this point, we know we really got a new mp */
		np = 0;
		nmp++;
		strcpy (mp[nmp].wwid, all_paths[k].wwid);
		PINDEX(nmp,np) = k;

		if (mp[nmp].size == 0)
			mp[nmp].size = get_disk_size (all_paths[k].dev);

		for (i = k + 1; i < conf->max_devs; i++) {
			if (0 == strcmp (all_paths[k].wwid, all_paths[i].wwid)) {
				np++;
				PINDEX(nmp,np) = i;
				mp[nmp].npaths = np;
			}
		}
	}
	return nmp;
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

#define policyis(x) (all_paths[mp->pindex[0]].iopolicy == x && conf->iopolicy == -1) \
		    || conf->iopolicy == x

static int
setup_map (struct env * conf, struct path * all_paths,
	struct multipath * mp, int op)
{
	char params[255];
	char * params_p;

	params_p = &params[0];

	/* paths grouping policy selector */
	/* implementations in pgpolicies.c */

	if (policyis (MULTIBUS))
		one_group (mp, all_paths, params_p);

	if (policyis (FAILOVER))
		one_path_per_group (mp, all_paths, params_p);

	if (policyis (GROUP_BY_SERIAL))
		group_by_serial (mp, all_paths, params_p);

	if (policyis (GROUP_BY_TUR))
		group_by_tur (mp, all_paths, params_p);

	if (mp->size < 0)
		return 0;

	if (!conf->quiet) {

		if (op == DM_DEVICE_RELOAD)
			printf ("U:");

		if (op == DM_DEVICE_CREATE)
			printf ("N:");

		printf ("%s:0 %li %s%s\n",
			mp->wwid, mp->size, DM_TARGET, params);
	}

	if (op == DM_DEVICE_RELOAD)
		dm_simplecmd (DM_DEVICE_SUSPEND, mp->wwid);

	dm_addmap (op, mp->wwid, params, mp->size);

	if (op == DM_DEVICE_RELOAD)
		dm_simplecmd (DM_DEVICE_RESUME, mp->wwid);

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
	fprintf (stderr, "Usage: %s [-v|-q] [-d] [-m max_devs]\n",
		progname);
	fprintf (stderr,	"                   [-p failover|multibus|group_by_serial]\n" \
			"                   [device]\n" \
			"\n" \
			"\t-v\t\tverbose, print all paths and multipaths\n" \
			"\t-q\t\tquiet, no output at all\n" \
			"\t-d\t\tdry run, do not create or update devmaps\n" \
			"\t-m max_devs\tscan {max_devs} devices at most\n" \
			"\n" \
			"\t-p policy\tforce maps to specified policy :\n" \
			"\t   failover\t\t- 1 path per priority group\n" \
			"\t   multibus\t\t- all paths in 1 priority group\n" \
			"\t   group_by_serial\t- 1 priority group per serial\n" \
			"\t   group_by_tur\t\t- 1 priority group per TUR state\n" \
			"\n" \
			"\t-D maj min\tlimit scope to the device's multipath\n" \
			"\t\t\t(major minor device reference)\n"
			"\tdevice\t\tlimit scope to the device's multipath\n" \
			"\t\t\t(hotplug-style $DEVPATH reference)\n"
		);

	unlink (RUN);
	exit (1);
}

int
main (int argc, char *argv[])
{
	struct multipath * mp;
	struct path * all_paths;
	struct env conf;
	int i, k, nmp;
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
	conf.max_devs = MAX_DEVS;
	conf.dry_run = 0;	/* 1 == Do not Create/Update devmaps */
	conf.verbose = 0;	/* 1 == Print all_paths and mp */
	conf.quiet = 0;		/* 1 == Do not even print devmaps */
	conf.iopolicy = -1;	/* do not override defaults */
	conf.major = -1;
	conf.minor = -1;
	conf.hwtable = NULL;

	/* argv parser */
	for (i = 1; i < argc; ++i) {

		argis ("-v") {
			if (conf.quiet == 1)
				usage (argv[0]);
			conf.verbose = 1;
		}
		
		else argis ("-m") {
			conf.max_devs = atoi (argv[++i]);
			if (conf.max_devs < 2)
				usage (argv[0]);
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
		
		else argis ("-d")
			conf.dry_run = 1;
		
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

	/* if we have a config file read it (overwrite hwtable) */
	if (check_config () && conf.iopolicy < 0) {
		conf.hwtable = read_config (getuid_list);

		/* read_config went nuts, fall back to defaults */
		if (conf.hwtable == NULL) {
			setup_default_hwtable;
			conf.hwtable = default_hwtable_addr;
		}

	} else {
		setup_default_hwtable;
		conf.hwtable = default_hwtable_addr;
	}


	/* dynamic allocations */
	mp = malloc (conf.max_devs * sizeof (struct multipath));
	all_paths = malloc (conf.max_devs * sizeof (struct path));

	if (mp == NULL || all_paths == NULL) {
		fprintf (stderr, "can not allocate memory\n");
		unlink (RUN);
		exit (1);
	}

	if (sysfs_get_mnt_path (conf.sysfs_path, FILE_NAME_SIZE)) {
		get_all_paths_nosysfs (&conf, all_paths);
	} else {
		get_all_paths_sysfs (&conf, all_paths);
	}

	nmp = coalesce_paths (&conf, mp, all_paths);

	if (conf.verbose) {
		print_all_path (&conf, all_paths);
		print_all_mp (all_paths, mp, nmp);
	}

	/* last chance to quit before messing with devmaps */
	if (conf.dry_run) {
		unlink (RUN);
		exit (0);
	}

	if (conf.verbose)
		fprintf (stdout, "# device maps :\n");

	for (k=0; k<=nmp; k++) {
		if (map_present (mp[k].wwid)) {
			setup_map (&conf, all_paths, &mp[k], DM_DEVICE_RELOAD);
		} else {
			setup_map (&conf, all_paths, &mp[k], DM_DEVICE_CREATE);
		}
	}

	/* signal multipathd that new devmaps may have come up */
	signal_daemon ();
	
	/* free allocs */
	free (mp);
	free (all_paths);

	/* release runfile */
	unlink (RUN);

	exit (0);
}
