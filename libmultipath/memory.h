#ifndef _MEMORY_H
#define _MEMORY_H

#include <stdlib.h>
#include <string.h>

#define MALLOC(a) zalloc(a)
#define FREE(a) free(a)

extern void * zalloc (unsigned long size);

#endif /* _MEMORY_H */
