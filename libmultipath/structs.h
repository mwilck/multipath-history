#ifndef _STRUCTS_H
#define _STRUCTS_H

#define WWID_SIZE		64
#define SERIAL_SIZE		17
#define NODE_NAME_SIZE		19
#define PATH_STR_SIZE  		16
#define PARAMS_SIZE		1024
#define FILE_NAME_SIZE		256
#define CALLOUT_MAX_SIZE	128
#define BLK_DEV_SIZE		33

#define SCSI_VENDOR_SIZE	9
#define SCSI_PRODUCT_SIZE	17
#define SCSI_REV_SIZE		5

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
	char dev_t[BLK_DEV_SIZE];
	struct scsi_idlun scsi_id;
	struct sg_id sg_id;
	char wwid[WWID_SIZE];
	char vendor_id[SCSI_VENDOR_SIZE];
	char product_id[SCSI_PRODUCT_SIZE];
	char rev[SCSI_REV_SIZE];
	char serial[SERIAL_SIZE];
	char tgt_node_name[NODE_NAME_SIZE];
	unsigned long size;
	int state;
	int dmstate;
	int failcount;
	unsigned int priority;
	int claimed;
	char * getuid;
	char * getprio;
	int (*checkfn) (int, char *, void **);
	void * checker_context;
	struct multipath * mpp;
	int fd;
	
	/* configlet pointers */
	struct hwentry * hwe;
};

struct multipath {
	char wwid[WWID_SIZE];
	char * alias;
	int pgpolicy;
	int nextpg;
	int queuedio;
	int action;
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
	long id;
	int status;
	int priority;
	vector paths;
};

struct path * alloc_path (void);
struct multipath * alloc_multipath (void);
void free_path (struct path *);
void free_multipath (struct multipath *);
struct multipath * find_mp (vector mp, char * alias);
struct path * find_path_by_devt (vector pathvec, char * devt);
struct path * find_path_by_dev (vector pathvec, char * dev);

char sysfs_path[FILE_NAME_SIZE];

#endif
