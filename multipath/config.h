#ifndef _CONFIG_H
#define _CONFIG_H

#ifndef _VECTOR_H
#include "vector.h"
#endif

struct mpentry {
	char * wwid;
	char * selector;
	int selector_args;
	int iopolicy;
	char * getuid;
	char * alias;
};

struct hwentry {
	char * vendor;
	char * product;
	char * selector;
	int selector_args;
	int iopolicy;
	char * getuid;
	int checker_index;
};

struct config {
	int verbosity;
	int dry_run;
	int iopolicy_flag;
	int with_sysfs;
	int major;
	int minor;
	char * dev;
	int signal;

	char * udev_dir;
	char * default_selector;
	int default_selector_args;
	int default_iopolicy;
	char * default_getuid;
	char * default_getprio;

	vector mptable;
	vector hwtable;
	vector aliases;
	vector blist;
};

struct config * conf;

#endif
