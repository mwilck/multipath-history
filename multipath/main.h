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
#include <vector.h>
#include "dev_t.h"

/* global defs */
#define WWID_SIZE	64
#define SERIAL_SIZE	15
#define PATH_STR_SIZE   16
#define PARAMS_SIZE	256
#define FILE_NAME_SIZE	256
#define DEV_T_SIZE	32
#define CALLOUT_MAX_SIZE	128
#define DEF_TIMEOUT	60000
#define DM_TARGET	"multipath"
#define PIDFILE		"/var/run/multipathd.pid"
#define RUN		"/var/run/multipath.run"
#define CONFIGFILE	"/etc/multipath.conf"
#define DEFAULT_GETUID	"/sbin/scsi_id -g -u -s /block/%n"
#define DEFAULT_UDEVDIR	"/udev"
#define DEFAULT_SELECTOR	"round-robin 0"
#define DEFAULT_FEATURES	"0"
#define DEFAULT_HWHANDLER	"0"

/*
 * SCSI strings sizes
 */
#define SCSI_VENDOR_SIZE	9
#define SCSI_PRODUCT_SIZE	17
#define SCSI_REV_SIZE		5

#define ACT_NOTHING_STR		"unchanged"
#define ACT_RELOAD_STR		"reload"
#define ACT_SWITCHPG_STR	"switchpg"
#define ACT_CREATE_STR		"create"

enum actions {
	ACT_NOTHING,
	ACT_RELOAD,
	ACT_SWITCHPG,
	ACT_CREATE
};

enum pathstates {
	PSTATE_RESERVED,
	PSTATE_FAILED,
	PSTATE_ACTIVE
};

enum pgstates {
	PGSTATE_RESERVED,
	PGSTATE_ENABLED,
	PGSTATE_DISABLED,
	PGSTATE_ACTIVE
};

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
	char dev_t[DEV_T_SIZE];
	struct scsi_idlun scsi_id;
	struct sg_id sg_id;
	char wwid[WWID_SIZE];
	char vendor_id[SCSI_VENDOR_SIZE];
	char product_id[SCSI_PRODUCT_SIZE];
	char rev[SCSI_REV_SIZE];
	char serial[SERIAL_SIZE];
	int state;
	int dmstate;
	int failcount;
	unsigned int priority;
	int claimed;
	char * getuid;
	char * getprio;
	int (*checkfn) (char *, char *, void **);
};

struct multipath {
	char wwid[WWID_SIZE];
	char * alias;
	int pgpolicy;
	int nextpg;
	int queuedio;
	unsigned long size;
	vector paths;
	vector pg;
	char params[PARAMS_SIZE];
	char status[PARAMS_SIZE];
	char * selector;
	char * features;
	char * hwhandler;

	/* configlet pointers */
	struct mpentry * mpe;
	struct hwentry * hwe;
};

struct pathgroup {
	int id;
	int status;
	int priority;
	vector paths;
};

/* Build version */
#define PROG    "multipath"

#define VERSION_CODE 0x000309
#define DATE_CODE    0x050c04

#define MULTIPATH_VERSION(version)	\
	(version >> 16) & 0xFF,		\
	(version >> 8) & 0xFF,		\
	version & 0xFF

#define VERSION_STRING PROG" v%d.%d.%d (%.2d/%.2d, 20%.2d)\n",	\
                MULTIPATH_VERSION(VERSION_CODE),		\
                MULTIPATH_VERSION(DATE_CODE)

#endif
