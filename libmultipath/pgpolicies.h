#ifndef _PGPOLICIES_H
#define _PGPOLICIES_H

#if 0
#ifndef _MAIN_H
#include "main.h"
#endif
#endif

#define POLICY_NAME_SIZE 32

/* Storage controlers capabilities */
enum iopolicies { 
	IOPOLICY_RESERVED,
	FAILOVER,
	MULTIBUS,
	GROUP_BY_SERIAL,
	GROUP_BY_PRIO,
	GROUP_BY_NODE_NAME
};

int get_pgpolicy_id(char *);
void get_pgpolicy_name (char *, int);

/*
 * policies
 */
int one_path_per_group(struct multipath *);
int one_group(struct multipath *);
int group_by_serial(struct multipath *);
int group_by_prio(struct multipath *);
int group_by_node_name(struct multipath *);

#endif
