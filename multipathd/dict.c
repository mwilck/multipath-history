#include "parser.h"
#include "hwtable.h"

/* data handlers */
/* Global def handlers */
static void
polling_interval_handler(vector strvec)
{
	checkint = atoi(VECTOR_SLOT(strvec, 1));
}

static void
blacklist_handler(vector strvec)
{
	blist = vector_alloc();
}

static void
devnode_handler(vector strvec)
{
	char * buff;
	
	buff = set_value(strvec);
	vector_alloc_slot(blist);
	vector_set_slot(blist, buff);
}

static void
devices_handler(vector strvec)
{
	hwtable = vector_alloc();
}

static void
device_handler(vector strvec)
{
	struct hwentry * hwe;

	vector_alloc_slot(hwtable);
	hwe = malloc(sizeof(struct hwentry));
	vector_set_slot(hwtable, hwe);
}

static void
vendor_handler(vector strvec)
{
	struct hwentry * hwe = VECTOR_SLOT(hwtable, VECTOR_SIZE(hwtable) - 1);
	hwe->vendor = set_value(strvec);
}

static void
product_handler(vector strvec)
{
	struct hwentry * hwe = VECTOR_SLOT(hwtable, VECTOR_SIZE(hwtable) - 1);
	hwe->product = set_value(strvec);
}

static void
path_checker_handler(vector strvec)
{
	char * buff;
	int i = 0;
	struct hwentry * hwe = VECTOR_SLOT(hwtable, VECTOR_SIZE(hwtable) - 1);

	buff = set_value(strvec);
	while (checker_list[i].name) {
		if (0 == strncmp(checker_list[i].name, buff,
				 sizeof(checker_list[i].name))) {
			hwe->checker_index = i;
			break;
		}
		i++;
	}
	free(buff);
}
static void
hw_getuid_callout_handler(vector strvec)
{
	int i;
	char * bin;
	char * curbin;
	char * p;

	/*
	 * purge command line arguments
	 */
	curbin = set_value(strvec);
	p = curbin;

	while (*p != ' ' && *p != '\0')
		p++;

	*p = '\0';

	if (binvec == NULL)
		binvec = vector_alloc();
	/*
	 * if this callout is already stored in binvec, don't store it twice
	 */
	for (i = 0; i < VECTOR_SIZE(binvec); i++) {
		bin = VECTOR_SLOT(binvec, i);

		if (memcmp (bin, curbin, sizeof(curbin)) == 0)
			return;
	}
	/*
	 * else, store it
	 */
	vector_alloc_slot(binvec);
	vector_set_slot(binvec, curbin);
}

vector
init_keywords(void)
{
	keywords = vector_alloc();

	install_keyword_root("defaults", NULL);
	install_keyword("polling_interval", &polling_interval_handler);
	install_keyword_root("devnode_blacklist", &blacklist_handler);
	install_keyword("devnode", &devnode_handler);
	install_keyword_root("devices", &devices_handler);
	install_keyword("device", &device_handler);
	install_sublevel();
	install_keyword("vendor", &vendor_handler);
	install_keyword("product", &product_handler);
	install_keyword("path_checker", &path_checker_handler);
	install_keyword("getuid_callout", &hw_getuid_callout_handler);
	install_sublevel_end();

	return keywords;
}
