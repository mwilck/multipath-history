#ifndef HWTABLE_H
#define HWTABLE_H

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

struct hwentry {
	char * vendor;
	char * product;
	int iopolicy;
	int (*getuid) (char *, char *);
};

/* External vars */ 
vector hwtable;
vector blist;

#define setup_default_hwtable struct hwentry defhwtable[] = { \
	{"COMPAQ  ", "HSV110 (C)COMPAQ", GROUP_BY_TUR, &get_evpd_wwid}, \
	{"COMPAQ  ", "MSA1000         ", GROUP_BY_TUR, &get_evpd_wwid}, \
	{"COMPAQ  ", "MSA1000 VOLUME  ", GROUP_BY_TUR, &get_evpd_wwid}, \
	{"DEC     ", "HSG80           ", GROUP_BY_TUR, &get_evpd_wwid}, \
	{"HP      ", "HSV100          ", GROUP_BY_TUR, &get_evpd_wwid}, \
	{"HP      ", "A6189A          ", MULTIBUS, &get_evpd_wwid}, \
	{"HP      ", "OPEN-           ", MULTIBUS, &get_evpd_wwid}, \
	{"DDN     ", "SAN DataDirector", MULTIBUS, &get_evpd_wwid}, \
	{"FSC     ", "CentricStor     ", MULTIBUS, &get_evpd_wwid}, \
	{"HITACHI ", "DF400           ", MULTIBUS, &get_evpd_wwid}, \
	{"HITACHI ", "DF500           ", MULTIBUS, &get_evpd_wwid}, \
	{"HITACHI ", "DF600           ", MULTIBUS, &get_evpd_wwid}, \
	{"IBM     ", "ProFibre 4000R  ", MULTIBUS, &get_evpd_wwid}, \
	{"SGI     ", "TP9100          ", MULTIBUS, &get_evpd_wwid}, \
	{"SGI     ", "TP9300          ", MULTIBUS, &get_evpd_wwid}, \
	{"SGI     ", "TP9400          ", MULTIBUS, &get_evpd_wwid}, \
	{"SGI     ", "TP9500          ", MULTIBUS, &get_evpd_wwid}, \
	{"3PARdata", "VV              ", GROUP_BY_TUR, &get_evpd_wwid}, \
	{"", "", 0, NULL}, \
}; \

#define default_hwtable_addr &defhwtable[0]

#endif
