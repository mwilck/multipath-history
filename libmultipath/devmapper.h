int dm_prereq (char *, int, int, int);
int dm_simplecmd (int, const char *);
int dm_addmap (int, const char *, const char *, const char *, unsigned long);
int dm_map_present (char *);
int dm_get_map(char *, unsigned long *, char **);
int dm_get_status(char *, char **);
int dm_type(char *, char *);
int dm_flush_maps (char *);
