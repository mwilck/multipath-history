#ifndef _BLACKLIST_H
#define _BLACKLIST_H

#define BLIST_ENTRY_SIZE 255

#define VECTOR_ADDSTR(a, b) \
	str = zalloc(6 * sizeof(char)); \
	snprintf(str, 6, b); \
	vector_alloc_slot(a); \
	vector_set_slot(a, str);

void setup_default_blist (vector blist);
int blacklist (vector blist, char * dev);

#endif /* _BLACKLIST_H */
