#ifndef _CONFIG_H
#define _CONFIG_H

#ifndef _VECTOR_H
#include "vector.h"
#endif

struct mpentry {
	int selector_args;
	int pgpolicy;

	char * wwid;
	char * selector;
	char * getuid;
	char * alias;
};

struct hwentry {
	int selector_args;
	int pgpolicy;
	int checker_index;

	char * vendor;
	char * product;
	char * selector;
	char * getuid;
	int checker_index;
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

	char * devt;
	char * dev;
	char * udev_dir;
	char * default_selector;
	char * default_getuid;
	char * default_getprio;

	vector mptable;
	vector hwtable;
	vector aliases;
	vector blist;
};

struct config * conf;

#endif
