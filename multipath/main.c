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

#include "main.h"
#include "devinfo.h"
#include "config.h"
#include "pgpolicies.h"
#include "dict.h"
#include "debug.h"
#include "dmparser.h"
#include "propsel.h"

/* helpers */
#define argis(x) if (0 == strcmp(x, argv[i]))

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
		vector_alloc_slot(pathvec);
		vector_set_slot(pathvec, curpath);
	}
	sysfs_close_directory(sdir);
	return 0;
}

/*
 * print_path styles
 */
#define PRINT_PATH_ALL		0
#define PRINT_PATH_SHORT	1

static void
print_path (struct path * pp, int style)
{
	if (style != PRINT_PATH_SHORT && pp->wwid)
		printf ("%s ", pp->wwid);
	else
		printf ("  \\_ ");

	printf("%i:%i:%i:%i ",
	       pp->sg_id.host_no,
	       pp->sg_id.channel,
	       pp->sg_id.scsi_id,
	       pp->sg_id.lun);

	if (pp->dev)
		printf("%-4s ", pp->dev);

	if (pp->dev_t)
		printf("%-7s ", pp->dev_t);

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
	switch (pp->dmstate) {
	case PSTATE_ACTIVE:
		printf("[active]");
		break;
	case PSTATE_FAILED:
		printf("[failed]");
		break;
	default:
		break;
	}
	if (pp->claimed)
		printf("[claimed]");

	if (style != PRINT_PATH_SHORT && pp->product_id)
		printf("[%.16s]", pp->product_id);

	fprintf(stdout, "\n");
}

#if DEBUG
static void
print_map (struct multipath * mpp)
{
	if (mpp->size && mpp->params)
		printf("0 %lu %s %s\n",
			 mpp->size, DM_TARGET, mpp->params);
	return;
}

static void
print_all_paths (vector pathvec)
{
	int i;
	char empty_buff[WWID_SIZE];
	struct path * pp;

	/* initialize a cmp 0-filled buffer */
	memset(empty_buff, 0, WWID_SIZE);

	vector_foreach_slot (pathvec, pp, i) {
		/* leave out paths with incomplete devinfo */
		if (memcmp(empty_buff, pp->wwid, WWID_SIZE) == 0)
			continue;

		print_path(pp, PRINT_PATH_ALL);
	}
}

static void
print_all_maps (vector mp)
{
	int i;
	struct multipath * mpp;

	vector_foreach_slot (mp, mpp, i)
		print_map(mpp);
}
#endif

static void
print_mp (struct multipath * mpp)
{
	int j, i;
	struct path * pp = NULL;
	struct pathgroup * pgp = NULL;

	if (mpp->alias)
		printf("%s", mpp->alias);

	if (conf->verbosity == 1) {
		printf("\n");
		return;
	}
	if (strncmp(mpp->alias, mpp->wwid, WWID_SIZE))
		printf(" (%s)", mpp->wwid);

	printf("\n");

	if (mpp->size)
		printf("[size=%lu MB]", mpp->size / 2 / 1024);

	if (mpp->features)
		printf("[features=\"%s\"]", mpp->features);

	if (mpp->hwhandler)
		printf("[hwhandler=\"%s\"]", mpp->hwhandler);

	fprintf(stdout, "\n");

	if (!mpp->pg)
		return;

	vector_foreach_slot (mpp->pg, pgp, j) {
		printf("\\_ ");

		if (mpp->selector)
			printf("%s ", mpp->selector);

		if (mpp->nextpg && mpp->nextpg == j + 1)
			printf("[first]");

		switch (pgp->status) {
		case PGSTATE_ENABLED:
			printf("[enabled]\n");
			break;
		case PGSTATE_DISABLED:
			printf("[disabled]\n");
			break;
		case PGSTATE_ACTIVE:
			printf("[active]\n");
			break;
		default:
			printf("\n");
			break;
		}
		vector_foreach_slot (pgp->paths, pp, i)
			print_path(pp, PRINT_PATH_SHORT);
	}
	printf("\n");
}

static void
print_all_mp (vector mp)
{
	int k;
	struct multipath * mpp;

	vector_foreach_slot (mp, mpp, k) 
		print_mp(mpp);
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

		mpp->mpe = find_mp(pp1->wwid);
		mpp->hwe = find_hw(pp1->vendor_id, pp1->product_id);

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
dm_switchgroup(char * mapname, int index)
{
	int r = 0;
	struct dm_task *dmt;
	char str[24];

	if (!(dmt = dm_task_create(DM_DEVICE_TARGET_MSG)))
		return 0;

	if (!dm_task_set_name(dmt, mapname))
		goto out;

	if (!dm_task_set_sector(dmt, 0))
		goto out;

	snprintf(str, 24, "switch_group %i\n", index);
	dbg("message %s 0 %s", mapname, str);

	if (!dm_task_set_message(dmt, str))
		goto out;

	if (!dm_task_run(dmt))
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

	sz = strlen(path) + 16;
	str = zalloc(sz);

	snprintf(str, sz, "reinstate_path %s\n", path);

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
	
	vector_foreach_slot (mp->pg, pgp, i) {
		pgp = VECTOR_SLOT(mp->pg, i);
		shift = snprintf(p, freechar, " %s %i 1", mp->selector,
				 VECTOR_SIZE(pgp->paths));
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
	 * apply selected grouping policy to valid paths
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
	 * ponders each path group and determine highest prio pg
	 */
	mpp->nextpg = 1;
	vector_foreach_slot (mpp->pg, pgp, i) {
		vector_foreach_slot (pgp->paths, pp, j) {
			pgp->id ^= (int)pp;
			if (pp->state != PATH_DOWN)
				pgp->priority += pp->priority;
		}
		if (pgp->priority > highest) {
			highest = pgp->priority;
			mpp->nextpg = i + 1;
		}
	}

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
		return 0;

	if (action == ACT_NOTHING)
		return 0;

	if (action == ACT_SWITCHPG) {
		dm_switchgroup(mpp->alias, mpp->nextpg);
		return 0;
	}
		
	/*
	 * device mapper creation or updating
	 * here we know we'll have garbage on stderr from
	 * libdevmapper. so shut it down temporarily.
	 */
	dm_log_init_verbose(0);

	if (op == DM_DEVICE_RELOAD) {
		if (conf->dev && filepresent(conf->dev) &&
		    dm_get_map(mapname, NULL, &curparams) &&
		    0 == strncmp(mpp->params, curparams, strlen(mpp->params))) {
	                pp = zalloc(sizeof(struct path));
	                basename(conf->dev, pp->dev);

	                if (devinfo(pp)) {
				r = 1;
	                        goto out;
			}
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

	if (!dm_addmap(op, mapname, DM_TARGET, mpp->params, mpp->size)) {
		dm_simplecmd(DM_DEVICE_REMOVE, mapname);
		goto out;
	}

	if (op == DM_DEVICE_RELOAD)
		if (!dm_simplecmd(DM_DEVICE_RESUME, mapname))
			goto out;

	out:
	dm_log_init_verbose(1);
	free(curparams);
	return r;
}

static int
dm_get_maps (vector mp, char * type)
{
	struct multipath * mpp;
	int r = 0;
	struct dm_task *dmt;
	struct dm_names *names;
	unsigned next = 0;
	unsigned long length;
	char *params;

	if (!(dmt = dm_task_create (DM_DEVICE_LIST)))
		return 0;

	if (!dm_task_run (dmt))
		goto out;

	if (!(names = dm_task_get_names (dmt)))
		goto out;

	if (!names->dev)
		goto out;

	do {
		if (dm_type(names->name, DM_TARGET)) {
			dm_get_map(names->name, &length, &params);
			mpp = zalloc(sizeof(struct multipath));

			if (!mpp) {
				r = 1;
				goto out;
			}
			mpp->size = length;
			strncat(mpp->wwid, names->name, WWID_SIZE);
			strncat(mpp->params, params, PARAMS_SIZE);

			vector_alloc_slot(mp);
			vector_set_slot(mp, mpp);
			mpp = NULL;
		}
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
		"\t-F\t\tflush all multipath device maps\n" \
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
	vector mp, curmp;
	vector pathvec;
	struct multipath * mpp;
	int k;
	int arg;
	extern char *optarg;
	extern int optind;

	if (dm_prereq(DM_TARGET, 1, 0, 3)) {
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
	conf->default_features = NULL;
	conf->default_hwhandler = NULL;

	while ((arg = getopt(argc, argv, ":qdlFSi:v:p:D:")) != EOF ) {
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
		case 'F':
			dm_flush_maps(DM_TARGET);
			goto out;
			break;
		case 'l':
			conf->list = 1;
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
	 * allocate the two core vectors to store paths and multipaths
	 */
	mp = vector_alloc();
	curmp = vector_alloc();
	pathvec = vector_alloc();

	if (!mp || !curmp || !pathvec) {
		fprintf(stderr, "can not allocate memory\n");
		exit(1);
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
	 * get a path list and group them as multipaths
	 */
	if (get_pathvec_sysfs(pathvec))
		exit(1);

	dm_get_maps(curmp, DM_TARGET);

	if (VECTOR_SIZE(pathvec) == 0 && conf->verbosity > 0) {
		fprintf(stdout, "no path found\n");
		exit(0);
	}

	vector_foreach_slot (curmp, mpp, k) {
		dbg("params = %s", mpp->params);
		dbg("status = %s", mpp->status);
		disassemble_map(pathvec, mpp->params, mpp);
		disassemble_status(mpp->status, mpp);
		reinstate_paths(mpp);
	}
	if (conf->list) {
		print_all_mp(curmp);
		goto out;
	}

	coalesce_paths(mp, pathvec);

	/*
	 * conf->dev can be a mapname
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

#if DEBUG
	fprintf(stdout, "#\n# all paths :\n#\n");
	print_all_paths(pathvec);
	fprintf(stdout, "#\n# device maps :\n#\n");
	print_all_maps(mp);
#endif
out:
	/*
	 * signal multipathd that new devmaps may have come up
	 */
	if (conf->signal)
		signal_daemon();
	
	/*
	 * free allocs : FIXME, need a freemp() or nothing
	 */
	free(mp);
	free(curmp);
	free(pathvec);

	exit(0);
}
