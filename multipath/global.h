#ifndef GLOBAL_H
#define GLOBAL_H

#include "devinfo.h"
#include "vector.h"

/* Storage controlers capabilities */
#define FAILOVER	0
#define MULTIBUS	1
#define GROUP_BY_SERIAL	2
#define GROUP_BY_TUR	3

/* lists */
static struct {
	char * name;
	int iopolicy;
} iopolicy_list[] = {
	{"failover", FAILOVER},
	{"multibus", MULTIBUS},
	{"group_by_serial", GROUP_BY_SERIAL},
	{"group_by_tur", GROUP_BY_TUR},
	{NULL, -1},
};

static struct {
	char * name;
	int (*getuid) (char *, char *);
} getuid_list[] = {
	{"get_null_uid", &get_null_uid},
	{"get_evpd_wwid", &get_evpd_wwid},
	{NULL, NULL},
};

struct mpentry {
	char * wwid;
	char * selector;
	int selector_args;
	int iopolicy;
	int (*getuid) (char *, char *);
};

struct hwentry {
	char * vendor;
	char * product;
	char * selector;
	int selector_args;
	int iopolicy;
	int (*getuid) (char *, char *);
};

/* Global vars */ 
struct config {
	int verbose;
	int quiet;
	int dry_run;
	int iopolicy;
	int with_sysfs;
	int major;
	int minor;
	char * hotplugdev;
	int signal;

	char * default_selector;
	int default_selector_args;

	vector mptable;
	vector hwtable;
	vector blist;
};

struct config * conf;

#endif
