/*
 * mdadm - manage Linux "md" devices aka RAID arrays.
 *
 * Copyright (C) 2001-2002 Neil Brown <neilb@cse.unsw.edu.au>
 *
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *    Author: Neil Brown
 *    Email: <neilb@cse.unsw.edu.au>
 *    Paper: Neil Brown
 *           School of Computer Science and Engineering
 *           The University of New South Wales
 *           Sydney, 2052
 *           Australia
 */

#include	"mdadm.h"
#include	"../libdevmapper/libdevmapper.h"
#include	<endian.h>
#include	<sysfs/libsysfs.h>
#include	<sysfs/dlist.h>

#if ! defined(__BIG_ENDIAN) && ! defined(__LITTLE_ENDIAN)
#error no endian defined
#endif
#include	"md_u.h"
#include	"md_p.h"

#define DEBUG 2
#define LOG(x, y, z...) if (DEBUG >= x) fprintf (stderr, y, ##z)

#define LINEAR	-1
#define STRIPED	0
#define RAID1	1

struct device {
	char devname[FILE_NAME_SIZE];
	unsigned long long size;
	mdp_super_t super;
};

static int
blacklist (char * dev) {
	int i;
	static struct {
		char * headstr;
		int lengh;
	} blist[] = {
		{"md", 2},
		{"dm", 2},
		{"sr", 2},
		{"scd", 3},
		{"ram", 3},
		{"raw", 3},
		{NULL, 0},
	};

	for (i = 0; blist[i].lengh; i++) {
		if (strncmp (dev, blist[i].headstr, blist[i].lengh) == 0)
			return 1;
	}

	return 0;
}

static struct device *
sort_by_disknum (struct device * a, struct device * b)
{
	return (a->super.this_disk.number < b->super.this_disk.number ? a : b);
}

static void
format_devname (char * devname)
{
	char * p = devname;

	while (*p) {
		if (*p == '!')
			*p = '/';
		p++;
	}
}

unsigned long long
get_disk_size (char * sysfs_path, char * devname) {
	unsigned long long size;
	char attr_path[FILE_NAME_SIZE];
	char buff[FILE_NAME_SIZE];

	sprintf(attr_path, "%s/%s/size", sysfs_path, devname);

	if (0 > sysfs_read_attribute_value(attr_path, buff,
		FILE_NAME_SIZE * sizeof(char)))
		return -1;
	
	size = atoi(buff);
	return size;
}

static int
linear_target (struct dlist * mddevs)
{
	char mapname[FILE_NAME_SIZE];
	char params[DM_PARAMS_SIZE];
	struct device * dev = NULL;
	unsigned long long start = 0;
	struct dm_task *dmt = NULL;

	dlist_for_each_data (mddevs, dev, struct device) {
		memset (&params, 0x0, DM_PARAMS_SIZE);

		if (!start) {
			/* do only on first iteration */
			sprintf (mapname, "%x:%x:%x:%x",
				 dev->super.set_uuid0,
				 dev->super.set_uuid1,
				 dev->super.set_uuid2,
				 dev->super.set_uuid3);
			
			if (!(dmt = dm_task_create (DM_DEVICE_CREATE)))
				return 1;

			if (!dm_task_set_name (dmt, mapname))
				goto out;
		}

		sprintf (params, " %i:%i 0",
				 dev->super.this_disk.major,
				 dev->super.this_disk.minor);

		LOG (1, "%llu %llu linear %s\n", start,
		     MD_NEW_SIZE_SECTORS(dev->size), params);

		if (!dm_task_add_target (dmt, start,
					 MD_NEW_SIZE_SECTORS(dev->size),
					 "linear", params))
			goto out;

		start += MD_NEW_SIZE_SECTORS(dev->size);
	}

	if (!dm_task_run (dmt))
		goto out;

	out:
	dm_task_destroy (dmt);
	return 0;
}

static int
striped_target (struct dlist * mddevs)
{
	char * p;
	char params[DM_PARAMS_SIZE];
	char mapname[FILE_NAME_SIZE];
	struct device * dev = NULL;
	unsigned long long size = 0;
	struct dm_task *dmt = NULL;

	p = &params[0];

	dlist_for_each_data (mddevs, dev, struct device) {

		if (!size) {
			/* do only on first iteration */
			sprintf (mapname, "%x:%x:%x:%x",
				 dev->super.set_uuid0,
				 dev->super.set_uuid1,
				 dev->super.set_uuid2,
				 dev->super.set_uuid3);

			size = dev->super.size * 2;
			p += sprintf (p, "%i", dev->super.nr_disks);
			p += sprintf (p, " %i", dev->super.chunk_size / 1024);
		}

		p += sprintf (p, " %i:%i 0",
				 dev->super.this_disk.major,
				 dev->super.this_disk.minor);
	}

	LOG (1, "0 %llu striped %s\n", size, params);

	if (!(dmt = dm_task_create (DM_DEVICE_CREATE)))
		return 1;

	if (!dm_task_set_name (dmt, mapname))
		goto out;

	if (!dm_task_add_target (dmt, 0, size, "striped", params))
		goto out;

	if (!dm_task_run (dmt))
		goto out;

	out:
	dm_task_destroy (dmt);
	return 0;
}

static int
raid1_target (struct dlist * mddevs)
{
	char * p;
	char params[DM_PARAMS_SIZE];
	char mapname[FILE_NAME_SIZE];
	struct device * dev = NULL;
	unsigned long long size = 0;
	struct dm_task *dmt = NULL;

	p = &params[0];

	dlist_for_each_data (mddevs, dev, struct device) {

		if (!size) {
			/* do only on first iteration */
			sprintf (mapname, "%x:%x:%x:%x",
				 dev->super.set_uuid0,
				 dev->super.set_uuid1,
				 dev->super.set_uuid2,
				 dev->super.set_uuid3);

			size = dev->super.size * 2;
			p += sprintf (p, "core 1 1024 %i", dev->super.nr_disks);
		}

		p += sprintf (p, " %i:%i 0",
				 dev->super.this_disk.major,
				 dev->super.this_disk.minor);
	}

	LOG (1, "0 %llu mirror %s\n", size, params);

	if (!(dmt = dm_task_create (DM_DEVICE_CREATE)))
		return 1;

	if (!dm_task_set_name (dmt, mapname))
		goto out;

	if (!dm_task_add_target (dmt, 0, size, "mirror", params))
		goto out;

	if (!dm_task_run (dmt))
		goto out;

	out:
	dm_task_destroy (dmt);
	return 0;
}

int main (int argc, char **argv)
{
	struct sysfs_directory * sdir;
	struct sysfs_directory * devp;
	char sysfs_path[FILE_NAME_SIZE];
	char devname[FILE_NAME_SIZE];
	struct device * dev = NULL;
	struct device * refdev = NULL;
	struct dlist * devlist = NULL;
	struct dlist * mddevs = NULL;
	int fd, err;
	int level;

	/*
	 * store every block device listed in sysfs in a dlist
	 * if it has a MD superblock
	 */
	if (sysfs_get_mnt_path (sysfs_path, FILE_NAME_SIZE)) {
		LOG (0, "need sysfs\n");
		exit (1);
	}

	sprintf (devname, "%s/block", sysfs_path);
	strncpy (sysfs_path, devname, FILE_NAME_SIZE);
	sdir = sysfs_open_directory (sysfs_path);
	sysfs_read_directory (sdir);

	dlist_for_each_data (sdir->subdirs, devp, struct sysfs_directory) {
		format_devname (devp->name);
		
		if (blacklist (devp->name))
			continue;

		LOG (2, "%s\n", devp->name);
		sprintf (devname, "/dev/%s", devp->name);
		fd = open(devname, O_RDONLY);

		if (fd < 0) {
			LOG (0, "Can't open %s\n", devname);
			close (fd);
			continue;
		}
		
		dev = malloc (sizeof (struct device));

		if (dev == NULL) {
			LOG (0, "can't allocate memory for device\n");
			exit (1);
		}
			
		err = load_super(fd, &dev->super);
		close (fd);

		switch(err) {
		case 1:
			LOG (2, "cannot find device size for %s: %s\n",
				devname, strerror(errno));
			free (dev);
			continue;
		case 2:
			LOG (2, "%s is too small for md\n",
				devname);
			free (dev);
			continue;
		case 3:
			LOG (2, "Cannot seek to superblock on %s: %s\n",
				devname, strerror(errno));
			free (dev);
			continue;
		case 4:
			LOG (2, "Cannot read superblock on %s\n",
				devname);
			free (dev);
			continue;
		case 5:
			LOG (2, "No super block found on %s "
				"(Expected magic %08x, got %08x)\n",
				devname, MD_SB_MAGIC, dev->super.md_magic);
			free (dev);
			continue;
		case 6:
			LOG (2, "Cannot interpret superblock on %s - "
				"version is %d\n",
				devname, dev->super.major_version);
			free (dev);
			continue;
		}

		/* here we got a valid raid member, store */
		LOG (1, "%s is part of a raid array\n", devname);

		if (devlist == NULL) {
			LOG (2, "First super detected : create dlist\n");
			devlist = dlist_new (sizeof (struct device));
		}

		strcpy (dev->devname, devname);
		dev->size = get_disk_size (sysfs_path, devp->name);
		dlist_push (devlist, dev);
	}

	sysfs_close_directory (sdir);

	/*
	 * we need at least one superblock to continue
	 */
	if (devlist == NULL) {
		LOG (1, "no superblocks found\n");
		exit (0);
	}
	
	/*
	 * coalesce by MD UUID
	 */
	LOG (2, "start coalescing\n");
	dlist_start (devlist);

	while (dlist_next (devlist)) {
		refdev = dlist_pop (devlist);
		mddevs = dlist_new (sizeof (struct device));
		dlist_push (mddevs, refdev);
		level = refdev->super.level;
		
		LOG (1, "raid found : %d:%d",
		     refdev->super.this_disk.major,
		     refdev->super.this_disk.minor);

		dlist_for_each_data (devlist, dev, struct device) {
			if (0 == compare_super (&refdev->super, &dev->super)) {
				LOG (1, " %d:%d",
				     dev->super.this_disk.major,
				     dev->super.this_disk.minor);
				dlist_insert_sorted (mddevs, dev,
						     (void *)sort_by_disknum);
				dlist_pop (devlist);
				continue;
			}
		}

		LOG (1, "\n");

		/*
		 * here we have an array in mddevs.
		 * do sanity checks and create devmap
		 */
		switch (level) {
		case LINEAR:
			linear_target (mddevs);
			break;
			
		case STRIPED:
			striped_target (mddevs);
			break;

		case RAID1:
			raid1_target (mddevs);
			break;

		default:
			LOG (1, "unsupported raid level : skip\n");
		}

		dlist_destroy (mddevs);
	}
	
	return 0;
}
