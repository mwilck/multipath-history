#ifndef _PGPOLICIES_H
#define _PGPOLICIES_H

#ifndef _MAIN_H
#include "main.h"
#endif

/* Storage controlers capabilities */
enum iopolicies { 
	FAILOVER,
	MULTIBUS,
	GROUP_BY_SERIAL,
	GROUP_BY_PRIO
};

int get_pgpolicy_id(char *);
void get_pgpolicy_name (char *, int);

void one_path_per_group(struct multipath *);
void one_group(struct multipath *);
void group_by_serial(struct multipath *, int);
void group_by_prio(struct multipath *);

#endif
