#ifndef _BLACKLIST_H
#define _BLACKLIST_H

#define BLIST_ENTRY_SIZE 255

struct blentry {
	char * str;
	void * preg;
};

#define VECTOR_ADDSTR(vec, string, len) \
	ble = zalloc(sizeof(struct blentry)); \
	ble->str = zalloc(len * sizeof(char)); \
	ble->preg = zalloc(sizeof(regex_t)); \
	snprintf(ble->str, len, string); \
	regcomp((regex_t *)ble->preg, ble->str, REG_EXTENDED|REG_NOSUB); \
	vector_alloc_slot(vec); \
	vector_set_slot(vec, ble);

void setup_default_blist (vector blist);
int blacklist (vector blist, char * dev);
void store_regex (vector blist, char * regex);

#endif /* _BLACKLIST_H */
