#include "memory.h"

void *
zalloc(unsigned long size)
{
	void *mem;

	if ((mem = malloc(size)))
		memset(mem, 0, size);
	
	return mem;
}
