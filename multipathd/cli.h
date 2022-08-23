enum {
	__LIST,
	__ADD,
	__DEL,
	__SWITCH,
	__SUSPEND,
	__RESUME,
	__REINSTATE,
	__FAIL,
	__PATHS,
	__MAPS,
	__PATH,
	__MAP,
	__GROUP,
	__RECONFIGURE,
	__STATUS,
	__STATS,
	__TOPOLOGY,
	__CONFIG,
	__BLACKLIST,
	__DEVICES,
	__FMT,
};

#define LIST		(1 << __LIST)
#define ADD		(1 << __ADD)
#define DEL		(1 << __DEL)
#define SWITCH		(1 << __SWITCH)
#define SUSPEND		(1 << __SUSPEND)
#define RESUME		(1 << __RESUME)
#define REINSTATE	(1 << __REINSTATE)
#define FAIL		(1 << __FAIL)
#define PATHS		(1 << __PATHS)
#define MAPS		(1 << __MAPS)
#define PATH		(1 << __PATH)
#define MAP		(1 << __MAP)
#define GROUP		(1 << __GROUP)
#define RECONFIGURE	(1 << __RECONFIGURE)
#define STATUS		(1 << __STATUS)
#define STATS		(1 << __STATS)
#define TOPOLOGY	(1 << __TOPOLOGY)
#define CONFIG		(1 << __CONFIG)
#define BLACKLIST	(1 << __BLACKLIST)
#define DEVICES  	(1 << __DEVICES)
#define FMT 	 	(1 << __FMT)

#define INITIAL_REPLY_LEN 1000

struct key {
	char * str;
	char * param;
	int code;
	int has_param;
};

struct handler {
	int fingerprint;
	int (*fn)(void *, char **, int *, void *);
};

vector keys;
vector handlers;

int alloc_handlers (void);
int add_handler (int fp, int (*fn)(void *, char **, int *, void *));
int set_handler_callback (int fp, int (*fn)(void *, char **, int *, void *));
int parse_cmd (char * cmd, char ** reply, int * len, void *);
int load_keys (void);
char * get_keyparam (vector v, int code);
void free_keys (vector vec);
void free_handlers (vector vec);
int cli_init (void);
char * key_generator (const char * str, int state);
