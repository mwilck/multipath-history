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

	char * dev;
	char * udev_dir;
	char * default_selector;
	char * default_getuid;
	char * default_getprio;
	char * default_features;
	char * default_hwhandler;

	vector mptable;
	vector hwtable;
	vector aliases;
	vector blist;
};

struct config * conf;

#endif
