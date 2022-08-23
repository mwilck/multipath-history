#ifndef _PRIO_H
#define _PRIO_H

/*
 * knowing about path struct gives flexibility to prioritizers
 */
#include "checkers.h"
#include "vector.h"
#include "structs.h"
#include "list.h"
#include "memory.h"

#define DEFAULT_PRIO "const"
#define DEFAULT_PRIO_ARGS ""

/*
 * Known prioritizers for use in hwtable.c
 */
#define PRIO_ALUA "alua"
#define PRIO_CONST "const"
#define PRIO_EMC "emc"
#define PRIO_HDS "hds"
#define PRIO_HP_SW "hp_sw"
#define PRIO_NETAPP "netapp"
#define PRIO_RANDOM "random"
#define PRIO_RDAC "rdac"
#define PRIO_DATACORE "datacore"

/*
 * Value used to mark the fact prio was not defined
 */
#define PRIO_UNDEF -1

/*
 * strings lengths
 */
#define LIB_PRIO_NAMELEN 255
#define PRIO_NAME_LEN 16
#define PRIO_ARGS_LEN 255

struct prio {
	struct list_head node;
	char name[PRIO_NAME_LEN];
	char args[PRIO_ARGS_LEN];
	int (*getprio)(struct path *, char *);
};

int init_prio (void);
void cleanup_prio (void);
struct prio * add_prio (char *);
struct prio * prio_lookup (char *);
int prio_getprio (struct prio *, struct path *);
char * prio_name (struct prio *);
char * prio_args (struct prio *);
int prio_set_args (struct prio *, char *);

#endif /* _PRIO_H */
