#include "parser.h"
#include "config.h"
#include "pgpolicies.h"
#include "debug.h"
#include "memory.h"

/*
 * default block handlers
 */
static void
udev_dir_handler(vector strvec)
{
	conf->udev_dir = set_value(strvec);
}

static void
def_selector_handler(vector strvec)
{
	conf->default_selector = set_value(strvec);
}

static void
def_selector_args_handler(vector strvec)
{
	char * buff;

	buff = set_value(strvec);
	conf->default_selector_args = atoi(buff);
}

static void
def_iopolicy_handler(vector strvec)
{
	char * buff;

	buff = set_value(strvec);
	conf->default_iopolicy = get_pgpolicy_id(buff);
	free(buff);
}

static void
def_getuid_callout_handler(vector strvec)
{
	conf->default_getuid = set_value(strvec);
}

static void
def_prio_callout_handler(vector strvec)
{
	conf->default_getprio = set_value(strvec);
}

/*
 * blacklist block handlers
 */
static void
blacklist_handler(vector strvec)
{
	conf->blist = vector_alloc();
}

static void
devnode_handler(vector strvec)
{
	char * buff;

	buff = set_value(strvec);
	vector_alloc_slot(conf->blist);
	vector_set_slot(conf->blist, buff);
}

/*
 * devices block handlers
 */
static void
devices_handler(vector strvec)
{
	conf->hwtable = vector_alloc();
}

static void
device_handler(vector strvec)
{
	struct hwentry * hwe;

	vector_alloc_slot(conf->hwtable);
	hwe = zalloc(sizeof(struct hwentry));
	vector_set_slot(conf->hwtable, hwe);
}

static void
vendor_handler(vector strvec)
{
	struct hwentry * hwe =
		VECTOR_SLOT(conf->hwtable, VECTOR_SIZE(conf->hwtable) - 1);
	hwe->vendor = set_value(strvec);
}

static void
product_handler(vector strvec)
{
	struct hwentry * hwe =
		VECTOR_SLOT(conf->hwtable, VECTOR_SIZE(conf->hwtable) - 1);
	hwe->product = set_value(strvec);
}

static void
hw_iopolicy_handler(vector strvec)
{
	char * buff;
	struct hwentry * hwe =
		VECTOR_SLOT(conf->hwtable, VECTOR_SIZE(conf->hwtable) - 1);

	buff = set_value(strvec);
	hwe->iopolicy = get_pgpolicy_id(buff);
	free(buff);
}

static void
hw_getuid_callout_handler(vector strvec)
{
	struct hwentry * hwe =
		VECTOR_SLOT(conf->hwtable, VECTOR_SIZE(conf->hwtable) - 1);

	hwe->getuid = set_value(strvec);
}

static void
hw_selector_handler(vector strvec)
{
	struct hwentry * hwe = VECTOR_SLOT(conf->hwtable, VECTOR_SIZE(conf->hwtable) - 1);
	
	hwe->selector = set_value(strvec);
}

static void
hw_selector_args_handler(vector strvec)
{
	char * buff;
	struct hwentry * hwe = VECTOR_SLOT(conf->hwtable, VECTOR_SIZE(conf->hwtable) - 1);

	buff = set_value(strvec);
	hwe->selector_args = atoi(buff);
}

/*
 * multipaths block handlers
 */
static void
multipaths_handler(vector strvec)
{
	conf->mptable = vector_alloc();
}

static void
multipath_handler(vector strvec)
{
	struct mpentry * mpe;

	vector_alloc_slot(conf->mptable);
	mpe = zalloc(sizeof(struct mpentry));
	vector_set_slot(conf->mptable, mpe);
}

static void
wwid_handler(vector strvec)
{
	struct mpentry * mpe = VECTOR_SLOT(conf->mptable,
					VECTOR_SIZE(conf->mptable) - 1);
	mpe->wwid = set_value(strvec);
}

static void
alias_handler(vector strvec)
{
        struct mpentry * mpe = VECTOR_SLOT(conf->mptable,
				       VECTOR_SIZE(conf->mptable) - 1);
        mpe->alias = set_value(strvec);
}

static void
mp_iopolicy_handler(vector strvec)
{
	char * buff;
	struct mpentry * mpe = VECTOR_SLOT(conf->mptable, VECTOR_SIZE(conf->mptable) - 1);

	buff = set_value(strvec);
	mpe->iopolicy = get_pgpolicy_id(buff);
	free(buff);
}

static void
mp_selector_handler(vector strvec)
{
	struct mpentry * mpe = VECTOR_SLOT(conf->mptable, VECTOR_SIZE(conf->mptable) - 1);
	
	mpe->selector = set_value(strvec);
}

static void
mp_selector_args_handler(vector strvec)
{
	char * buff;
	struct mpentry * mpe = VECTOR_SLOT(conf->mptable, VECTOR_SIZE(conf->mptable) - 1);

	buff = set_value(strvec);
	mpe->selector_args = atoi(buff);
}
			
vector
init_keywords(void)
{
	keywords = vector_alloc();

	install_keyword_root("defaults", NULL);
	install_keyword("udev_dir", &udev_dir_handler);
	install_keyword("default_selector", &def_selector_handler);
	install_keyword("default_selector_args", &def_selector_args_handler);
	install_keyword("default_path_grouping_policy", &def_iopolicy_handler);
	install_keyword("default_getuid_callout", &def_getuid_callout_handler);
	install_keyword("default_prio_callout", &def_prio_callout_handler);
	
	install_keyword_root("devnode_blacklist", &blacklist_handler);
	install_keyword("devnode", &devnode_handler);

	install_keyword_root("devices", &devices_handler);
	install_keyword("device", &device_handler);
	install_sublevel();
	install_keyword("vendor", &vendor_handler);
	install_keyword("product", &product_handler);
	install_keyword("path_grouping_policy", &hw_iopolicy_handler);
	install_keyword("getuid_callout", &hw_getuid_callout_handler);
	install_keyword("path_selector", &hw_selector_handler);
	install_keyword("path_selector_args", &hw_selector_args_handler);
	install_sublevel_end();

	install_keyword_root("multipaths", &multipaths_handler);
	install_keyword("multipath", &multipath_handler);
	install_sublevel();
	install_keyword("wwid", &wwid_handler);
	install_keyword("alias", &alias_handler);
	install_keyword("path_grouping_policy", &mp_iopolicy_handler);
	install_keyword("path_selector", &mp_selector_handler);
	install_keyword("path_selector_args", &mp_selector_args_handler);
	install_sublevel_end();

	return keywords;
}
