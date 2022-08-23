#ifndef _CONFIG_H
#define _CONFIG_H

#ifndef _VECTOR_H
#include "vector.h"
#endif

enum devtypes {
	DEV_NONE,
	DEV_DEVT,
	DEV_DEVNODE,
	DEV_DEVMAP
};

struct hwentry {
	int selector_args;
	int pgpolicy;
	int checker_index;

	char * vendor;
	char * product;
	char * selector;
	char * getuid;
	char * getprio;
	char * features;
	char * hwhandler;
};

struct mpentry {
	int selector_args;
	int pgpolicy;

	char * wwid;
	char * selector;
	char * getuid;
	char * alias;
};

struct config {
	int verbosity;
	int dry_run;
	int list;
	int signal;
	int pgpolicy_flag;
	int with_sysfs;
	int default_selector_args;
	int default_pgpolicy;
	int dev_type;
	int minio;
	int checkint;

	char * dev;
	char * multipath;
	char * udev_dir;
	char * default_selector;
	char * default_getuid;
	char * default_getprio;
	char * default_features;
	char * default_hwhandler;

	vector mptable;
	vector hwtable;
	vector blist;
	vector binvec;
};

struct config * conf;

struct hwentry * find_hwe (vector hwtable, char * vendor, char * product);
struct mpentry * find_mpe (char * wwid);
char * get_mpe_wwid (char * alias);

void free_hwe (struct hwentry * hwe);
void free_hwtable (vector hwtable);
void free_mpe (struct mpentry * mpe);
void free_mptable (vector mptable);

struct config * alloc_config (void);
void free_config (struct config * conf);

#endif
