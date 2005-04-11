#define CACHE_FILE	"/dev/.multipath.cache"
#define CACHE_TMPFILE	"/dev/.multipath.cache.swp"
#define CACHE_EXPIRE	5
#define MAX_WAIT	5

int cache_load (vector pathvec);
int cache_dump (vector pathvec);
int cache_cold (int expire);
