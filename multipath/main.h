/*
 * Soft:        Description here...
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

#ifndef _MAIN_H
#define _MAIN_H

/* local includes */
#include "vector.h"
#include "dev_t.h"

/* global defs */
#define WWID_SIZE	64
#define SERIAL_SIZE	15
#define PATH_STR_SIZE   16
#define PARAMS_SIZE	256
#define FILE_NAME_SIZE	256
#define DEF_TIMEOUT	60000
#define DM_TARGET	"multipath"
#define PIDFILE		"/var/run/multipathd.pid"
#define RUN		"/var/run/multipath.run"
#define CONFIGFILE	"/etc/multipath.conf"
#define DEFAULT_GETUID	"/sbin/scsi_id -g -u -s"
#define DEFAULT_UDEVDIR	"/udev"
#define DEFAULT_SELECTOR	"round-robin"

/*
 * SCSI strings sizes
 */
#define SCSI_VENDOR_SIZE	9
#define SCSI_PRODUCT_SIZE	17
#define SCSI_REV_SIZE		5

/* global types */
struct scsi_idlun {
	int dev_id;
	int host_unique_id;
	int host_no;
};

struct sg_id {
	int host_no;
	int channel;
	int scsi_id;
	int lun;
	int scsi_type;
	short h_cmd_per_lun;
	short d_queue_depth;
	int unused1;
	int unused2;
};

struct scsi_dev {
	char dev[FILE_NAME_SIZE];
	struct scsi_idlun scsi_id;
	int host_no;
};

struct path {
	char dev[FILE_NAME_SIZE];
	char dev_t[FILE_NAME_SIZE];
	char sg_dev_t[FILE_NAME_SIZE];
	struct scsi_idlun scsi_id;
	struct sg_id sg_id;
	char wwid[WWID_SIZE];
	char vendor_id[SCSI_VENDOR_SIZE];
	char product_id[SCSI_PRODUCT_SIZE];
	char rev[SCSI_REV_SIZE];
	char serial[SERIAL_SIZE];
	int state;
	unsigned int priority;
	int claimed;
	int (*checkfn) (char *);
};

struct multipath {
	char wwid[WWID_SIZE];
	char * alias;
	int iopolicy;
	unsigned long size;
	vector paths;
	vector pg;
	char params[PARAMS_SIZE];
};

/* Build version */
#define PROG    "multipath"

#define VERSION_CODE 0x000012
#define DATE_CODE    0x021504

#define MULTIPATH_VERSION(version)	\
	(version >> 16) & 0xFF,		\
	(version >> 8) & 0xFF,		\
	version & 0xFF

#define VERSION_STRING PROG" v%d.%d.%d (%.2d/%.2d, 20%.2d)\n",	\
                MULTIPATH_VERSION(VERSION_CODE),		\
                MULTIPATH_VERSION(DATE_CODE)

#endif
