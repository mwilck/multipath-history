#ifndef _PGPOLICIES_H
#define _PGPOLICIES_H

#ifndef _MAIN_H
#include "main.h"
#endif

#define POLICY_NAME_SIZE 32

/* Storage controlers capabilities */
enum iopolicies { 
	IOPOLICY_RESERVED,
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
