#ifndef _PGPOLICIES_H
#define _PGPOLICIES_H

#ifndef _MAIN__H
#include "main.h"
#endif


void assemble_map(struct multipath *);
void one_path_per_group(struct multipath *);
void one_group(struct multipath *);
void group_by_serial(struct multipath *, int);
#endif
