#ifndef _PGPOLICIES_H
#define _PGPOLICIES_H

#ifndef _MAIN__H
#include "main.h"
#endif

/* Storage controlers capabilities */
#define FAILOVER	0
#define MULTIBUS	1
#define GROUP_BY_SERIAL	2
#define GROUP_BY_PRIO	3

static char * iopolicies[] = {
	"failover",
	"multibus",
	"group_by_serial",
	"group_by_prio",
	NULL,
};

void one_path_per_group(struct multipath *);
void one_group(struct multipath *);
void group_by_serial(struct multipath *, int);
void group_by_prio(struct multipath *);

#endif
