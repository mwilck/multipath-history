#ifndef HWTABLE_H
#define HWTABLE_H
#define CHECKINT 5

#include "checkers.h"
#include "vector.h"

/* lists */
static struct {
	char * name;
	int (*checker) (char *);
} checker_list[] = {
	{"readsector0", &readsector0},
	{"tur", &tur},
	{NULL, NULL},
};

struct hwentry {
	char * vendor;
	char * product;
	int checker_index;
};

/* External vars */ 
vector hwtable;
vector blist;
int checkint;

#endif
