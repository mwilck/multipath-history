#ifndef _BLACKLIST_H
#define _BLACKLIST_H

#include "regex.h"

#define MATCH_NOTHING       0
#define MATCH_WWID_BLIST    1
#define MATCH_DEVICE_BLIST  2
#define MATCH_DEVNODE_BLIST 3
#define MATCH_WWID_BLIST_EXCEPT    -MATCH_WWID_BLIST
#define MATCH_DEVICE_BLIST_EXCEPT  -MATCH_DEVICE_BLIST
#define MATCH_DEVNODE_BLIST_EXCEPT -MATCH_DEVNODE_BLIST

struct blentry {
	char * str;
	regex_t regex;
	int origin;
};

struct blentry_device {
	char * vendor;
	char * product;
	regex_t vendor_reg;
	regex_t product_reg;
	int origin;
};

int setup_default_blist (struct config *);
int alloc_ble_device (vector);
int filter_devnode (vector, vector, char *);
int filter_wwid (vector, vector, char *);
int filter_device (vector, vector, char *, char *);
int filter_path (struct config *, struct path *);
int store_ble (vector, char *, int);
int set_ble_device (vector, char *, char *, int);
void free_blacklist (vector);
void free_blacklist_device (vector);

#endif /* _BLACKLIST_H */
