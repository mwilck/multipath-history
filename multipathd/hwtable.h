#ifndef HWTABLE_H
#define HWTABLE_H

#define CHECKINT 5
#define MULTIPATH "/sbin/multipath -v 0 -S"

#include "vector.h"

struct hwentry {
	char * vendor;
	char * product;
	int checker_index;
};

/* External vars */ 
vector hwtable;
vector blist;
vector binvec;
int checkint;
char * multipath;

#endif
