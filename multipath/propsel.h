struct mpentry * find_mpe (char * wwid);
char * get_mpe_wwid (char * alias);

int select_pgpolicy (struct multipath * mp);
int select_selector (struct multipath * mp);
int select_alias (struct multipath * mp);
int select_features (struct multipath * mp);
int select_hwhandler (struct multipath * mp);
int select_checkfn(struct path *pp);
int select_getuid (struct path * pp);
int select_getprio (struct path * pp);

