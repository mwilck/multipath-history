#ifndef _CHECKERS_H
#define _CHECKERS_H

#define CHECKER_NAME_SIZE 16
#define DEVNODE_SIZE 256

enum checkers {
	CHECKER_RESERVED,
	TUR,
	READSECTOR0,
	EMC_CLARIION
};

int checkpath (char *, void *);

int get_checker_id (char *);
void *get_checker_addr (int);
int get_checker_name (char *, int);

int emc_clariion (char *);
int readsector0 (char *);
int tur (char *);

#endif /* _CHECKERS_H */
