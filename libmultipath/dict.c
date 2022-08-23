#include "vector.h"
#include "hwtable.h"
#include "structs.h"
#include "parser.h"
#include "config.h"
#include "debug.h"
#include "memory.h"
#include "pgpolicies.h"
#include "blacklist.h"

#include "../libcheckers/checkers.h"

/*
 * helper function to draw a list of callout binaries found in the config file
 */
extern int
push_callout(char * callout)
{
	int i;
	char * bin;
	char * p;

	/*
	 * purge command line arguments
	 */
	p = callout;

	while (*p != ' ' && *p != '\0')
		p++;

	if (!conf->binvec)
		conf->binvec = vector_alloc();


	if (!conf->binvec)
		return 1;

	/*
	 * if this callout is already stored in binvec, don't store it twice
	 */
	vector_foreach_slot (conf->binvec, bin, i)
		if (memcmp(bin, callout, p - callout) == 0)
			return 0;

	/*
	 * else, store it
	 */
	bin = MALLOC((p - callout) + 1);

	if (!bin)
		return 1;

	strncpy(bin, callout, p - callout);

	if (!vector_alloc_slot(conf->binvec))
		return 1;

	vector_set_slot(conf->binvec, bin);

	return 0;
}

/*
 * default block handlers
 */
static int
multipath_tool_handler(vector strvec)
{
	conf->multipath = set_value(strvec);

	if (!conf->multipath)
		return 1;

	return push_callout(conf->multipath);
}

static int
polling_interval_handler(vector strvec)
{
	char * buff;

	buff = VECTOR_SLOT(strvec, 1);
	conf->checkint = atoi(buff);

	return 0;
}

static int
udev_dir_handler(vector strvec)
{
	conf->udev_dir = set_value(strvec);

	if (!conf->udev_dir)
		return 1;

	return 0;
}

static int
def_selector_handler(vector strvec)
{
	conf->default_selector = set_value(strvec);

	if (!conf->default_selector)
		return 1;

	return 0;
}

static int
def_pgpolicy_handler(vector strvec)
{
	char * buff;

	buff = set_value(strvec);

	if (!buff)
		return 1;

	conf->default_pgpolicy = get_pgpolicy_id(buff);
	FREE(buff);

	return 0;
}

static int
def_getuid_callout_handler(vector strvec)
{
	conf->default_getuid = set_value(strvec);

	if (!conf->default_getuid)
		return 1;
	
	return push_callout(conf->default_getuid);
}

static int
def_prio_callout_handler(vector strvec)
{
	conf->default_getprio = set_value(strvec);

	if (!conf->default_getprio)
		return 1;
	
	return push_callout(conf->default_getprio);
}

static int
def_features_handler(vector strvec)
{
	conf->default_features = set_value(strvec);

	if (!conf->default_features)
		return 1;

	return 0;
}

static int
def_minio_handler(vector strvec)
{
	char * buff;

	buff = set_value(strvec);

	if (!buff)
		return 1;

	conf->minio = atoi(buff);
	FREE(buff);

	return 0;
}

/*
 * blacklist block handlers
 */
static int
blacklist_handler(vector strvec)
{
	conf->blist = vector_alloc();

	if (!conf->blist)
		return 1;

	return 0;
}

static int
ble_handler(vector strvec)
{
	char * buff;
	int ret;

	buff = set_value(strvec);

	if (!buff)
		return 1;

	ret = store_regex(conf->blist, buff);
	FREE(buff);

	return ret;
}

/*
 * devices block handlers
 */
static int
devices_handler(vector strvec)
{
	conf->hwtable = vector_alloc();

	if (!conf->hwtable)
		return 1;

	return 0;
}

static int
device_handler(vector strvec)
{
	struct hwentry * hwe;

	hwe = (struct hwentry *)MALLOC(sizeof(struct hwentry));

	if (!hwe)
		return 1;

	if (!vector_alloc_slot(conf->hwtable)) {
		FREE(hwe);
		return 1;
	}
	vector_set_slot(conf->hwtable, hwe);

	return 0;
}

static int
vendor_handler(vector strvec)
{
	struct hwentry * hwe = VECTOR_LAST_SLOT(conf->hwtable);

	if (!hwe)
		return 1;
	
	hwe->vendor = set_value(strvec);

	if (!hwe->vendor)
		return 1;

	return 0;
}

static int
product_handler(vector strvec)
{
	struct hwentry * hwe = VECTOR_LAST_SLOT(conf->hwtable);

	if (!hwe)
		return 1;
	
	hwe->product = set_value(strvec);

	if (!hwe->product)
		return 1;

	return 0;
}

static int
hw_pgpolicy_handler(vector strvec)
{
	char * buff;
	struct hwentry * hwe = VECTOR_LAST_SLOT(conf->hwtable);

	buff = set_value(strvec);

	if (!buff)
		return 1;

	hwe->pgpolicy = get_pgpolicy_id(buff);
	FREE(buff);

	return 0;
}

static int
hw_getuid_callout_handler(vector strvec)
{
	struct hwentry * hwe = VECTOR_LAST_SLOT(conf->hwtable);

	hwe->getuid = set_value(strvec);

	if (!hwe->getuid)
		return 1;

	return push_callout(hwe->getuid);
}

static int
hw_selector_handler(vector strvec)
{
	struct hwentry * hwe = VECTOR_LAST_SLOT(conf->hwtable);
	
	if (!hwe)
		return 1;

	hwe->selector = set_value(strvec);

	if (!hwe->selector)
		return 1;

	return 0;
}

static int
hw_path_checker_handler(vector strvec)
{
	char * buff;
	struct hwentry * hwe = VECTOR_LAST_SLOT(conf->hwtable);

	if (!hwe)
		return 1;

	buff = set_value(strvec);

	if (!buff)
		return 1;
	
	hwe->checker_index = get_checker_id(buff);
	FREE(buff);

	return 0;
}

static int
hw_features_handler(vector strvec)
{
	struct hwentry * hwe = VECTOR_LAST_SLOT(conf->hwtable);
	
	if (!hwe)
		return 1;

	hwe->features = set_value(strvec);

	if (!hwe->features)
		return 1;

	return 0;
}

static int
hw_handler_handler(vector strvec)
{
	struct hwentry * hwe = VECTOR_LAST_SLOT(conf->hwtable);
	
	if (!hwe)
		return 1;

	hwe->hwhandler = set_value(strvec);

	if (!hwe->hwhandler)
		return 1;

	return 0;
}

static int
prio_callout_handler(vector strvec)
{
	struct hwentry * hwe = VECTOR_LAST_SLOT(conf->hwtable);
	
	if (!hwe)
		return 1;

	hwe->getprio = set_value(strvec);

	if (!hwe->getprio)
		return 1;

	return push_callout(hwe->getprio);
}

/*
 * multipaths block handlers
 */
static int
multipaths_handler(vector strvec)
{
	conf->mptable = vector_alloc();

	if (!conf->mptable)
		return 1;

	return 0;
}

static int
multipath_handler(vector strvec)
{
	struct mpentry * mpe;

	mpe = (struct mpentry *)MALLOC(sizeof(struct mpentry));

	if (!mpe)
		return 1;

	if (!vector_alloc_slot(conf->mptable)) {
		FREE(mpe);
		return 1;
	}
	vector_set_slot(conf->mptable, mpe);

	return 0;
}

static int
wwid_handler(vector strvec)
{
	struct mpentry * mpe = VECTOR_LAST_SLOT(conf->mptable);

	if (!mpe)
		return 1;

	mpe->wwid = set_value(strvec);

	if (!mpe->wwid)
		return 1;

	return 0;
}

static int
alias_handler(vector strvec)
{
	struct mpentry * mpe = VECTOR_LAST_SLOT(conf->mptable);

	if (!mpe)
		return 1;

        mpe->alias = set_value(strvec);

	if (!mpe->alias)
		return 1;

	return 0;
}

static int
mp_pgpolicy_handler(vector strvec)
{
	char * buff;
	struct mpentry * mpe = VECTOR_LAST_SLOT(conf->mptable);

	if (!mpe)
		return 1;

	buff = set_value(strvec);

	if (!buff)
		return 1;

	mpe->pgpolicy = get_pgpolicy_id(buff);
	FREE(buff);

	return 0;
}

static int
mp_selector_handler(vector strvec)
{
	struct mpentry * mpe = VECTOR_LAST_SLOT(conf->mptable);
	
	if (!mpe)
		return 1;

	mpe->selector = set_value(strvec);

	if (!mpe->selector)
		return 1;

	return 0;
}

vector
init_keywords(void)
{
	keywords = vector_alloc();

	install_keyword_root("defaults", NULL);
	install_keyword("polling_interval", &polling_interval_handler);
	install_keyword("multipath_tool", &multipath_tool_handler);
	install_keyword("udev_dir", &udev_dir_handler);
	install_keyword("default_selector", &def_selector_handler);
	install_keyword("default_path_grouping_policy", &def_pgpolicy_handler);
	install_keyword("default_getuid_callout", &def_getuid_callout_handler);
	install_keyword("default_prio_callout", &def_prio_callout_handler);
	install_keyword("default_features", &def_features_handler);
	install_keyword("rr_min_io", &def_minio_handler);
	
	install_keyword_root("devnode_blacklist", &blacklist_handler);
	install_keyword("devnode", &ble_handler);
	install_keyword("wwid", &ble_handler);

	install_keyword_root("devices", &devices_handler);
	install_keyword("device", &device_handler);
	install_sublevel();
	install_keyword("vendor", &vendor_handler);
	install_keyword("product", &product_handler);
	install_keyword("path_grouping_policy", &hw_pgpolicy_handler);
	install_keyword("getuid_callout", &hw_getuid_callout_handler);
	install_keyword("path_selector", &hw_selector_handler);
	install_keyword("path_checker", &hw_path_checker_handler);
	install_keyword("features", &hw_features_handler);
	install_keyword("hardware_handler", &hw_handler_handler);
	install_keyword("prio_callout", &prio_callout_handler);
	install_sublevel_end();

	install_keyword_root("multipaths", &multipaths_handler);
	install_keyword("multipath", &multipath_handler);
	install_sublevel();
	install_keyword("wwid", &wwid_handler);
	install_keyword("alias", &alias_handler);
	install_keyword("path_grouping_policy", &mp_pgpolicy_handler);
	install_keyword("path_selector", &mp_selector_handler);
	install_sublevel_end();

	return keywords;
}
