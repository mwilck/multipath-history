#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "memory.h"
#include "vector.h"
#include "util.h"
#include "blacklist.h"

void
setup_default_blist (vector blist)
{
	char * str;

	VECTOR_ADDSTR(blist, "c0d");
	VECTOR_ADDSTR(blist, "c1d");
	VECTOR_ADDSTR(blist, "c2d");
	VECTOR_ADDSTR(blist, "fd");
	VECTOR_ADDSTR(blist, "hd");
	VECTOR_ADDSTR(blist, "md");
	VECTOR_ADDSTR(blist, "dm");
	VECTOR_ADDSTR(blist, "sr");
	VECTOR_ADDSTR(blist, "scd");
	VECTOR_ADDSTR(blist, "st");
	VECTOR_ADDSTR(blist, "ram");
	VECTOR_ADDSTR(blist, "raw");
	VECTOR_ADDSTR(blist, "loop");
	VECTOR_ADDSTR(blist, "nbd");
	VECTOR_ADDSTR(blist, "ub");
}

int
blacklist (vector blist, char * dev)
{
	int i;
	int ref_dev_len, blist_dev_len;
	char *p;
	char buff[BLIST_ENTRY_SIZE];

	basename(dev, buff);
	ref_dev_len = strlen(buff);

	vector_foreach_slot (blist, p, i) {
		blist_dev_len = strlen(p);

		if (blist_dev_len > ref_dev_len)
			continue;

		if (memcmp(buff, p, blist_dev_len) == 0 &&
		    (blist_dev_len == ref_dev_len ||
		    isdigit(buff[blist_dev_len]))) {
			return 1;
		}
	}
	return 0;
}

