#if DEBUG
#define dbg(format, arg...) fprintf(stderr, format "\n", ##arg)
#else
#define dbg(format, arg...) do {} while(0)
#endif

#define safe_sprintf(var, format, args...)      \
	snprintf(var, sizeof(var), format, ##args) >= sizeof(var)
