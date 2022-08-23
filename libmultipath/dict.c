/*
 * Based on Alexandre Cassen template for keepalived
 * Copyright (c) 2004, 2005, 2006  Christophe Varoqui
 * Copyright (c) 2005 Benjamin Marzinski, Redhat
 * Copyright (c) 2005 Kiyoshi Ueda, NEC
 */
#include <sys/types.h>
#include <pwd.h>

#include "checkers.h"
#include "vector.h"
#include "hwtable.h"
#include "structs.h"
#include "parser.h"
#include "config.h"
#include "debug.h"
#include "memory.h"
#include "pgpolicies.h"
#include "blacklist.h"
#include "defaults.h"
#include "prio.h"
#include "errno.h"

/*
 * default block handlers
 */
static int
polling_interval_handler(vector strvec)
{
	char * buff;

	buff = VECTOR_SLOT(strvec, 1);
	conf->checkint = atoi(buff);
	conf->max_checkint = MAX_CHECKINT(conf->checkint);

	return 0;
}

static int
verbosity_handler(vector strvec)
{
	char * buff;

	buff = VECTOR_SLOT(strvec, 1);
	conf->verbosity = atoi(buff);

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
multipath_dir_handler(vector strvec)
{
	conf->multipath_dir = set_value(strvec);

	if (!conf->multipath_dir)
		return 1;

	return 0;
}

static int
def_selector_handler(vector strvec)
{
	conf->selector = set_value(strvec);

	if (!conf->selector)
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

	conf->pgpolicy = get_pgpolicy_id(buff);
	FREE(buff);

	return 0;
}

static int
def_getuid_callout_handler(vector strvec)
{
	conf->getuid = set_value(strvec);

	if (!conf->getuid)
		return 1;

	return 0;
}

static int
def_prio_handler(vector strvec)
{
	conf->prio_name = set_value(strvec);

	if (!conf->prio_name)
		return 1;

	return 0;
}

static int
def_features_handler(vector strvec)
{
	conf->features = set_value(strvec);

	if (!conf->features)
		return 1;

	return 0;
}

static int
def_path_checker_handler(vector strvec)
{
	conf->checker_name = set_value(strvec);

	if (!conf->checker_name)
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

static int
get_sys_max_fds(int *max_fds)
{
	FILE *file;
	int nr_open;
	int ret = 1;

	file = fopen("/proc/sys/fs/nr_open", "r");
	if (!file) {
		fprintf(stderr, "Cannot open /proc/sys/fs/nr_open : %s\n",
			strerror(errno));
		return 1;
	}
	if (fscanf(file, "%d", &nr_open) != 1) {
		fprintf(stderr, "Cannot read max open fds from /proc/sys/fs/nr_open");
		if (ferror(file))
			fprintf(stderr, " : %s\n", strerror(errno));
		else
			fprintf(stderr, "\n");
	} else {
		*max_fds = nr_open;
		ret = 0;
	}
	fclose(file);
	return ret;
}


static int
max_fds_handler(vector strvec)
{
	char * buff;
	int r = 0;

	buff = set_value(strvec);

	if (!buff)
		return 1;

	if (strlen(buff) == 3 &&
	    !strcmp(buff, "max"))
		r = get_sys_max_fds(&conf->max_fds);
	else
		conf->max_fds = atoi(buff);
	FREE(buff);

	return r;
}

static int
def_mode_handler(vector strvec)
{
	mode_t mode;
	char *buff;

	buff = set_value(strvec);

	if (!buff)
		return 1;

	if (sscanf(buff, "%o", &mode) == 1 && mode <= 0777) {
		conf->attribute_flags |= (1 << ATTR_MODE);
		conf->mode = mode;
	}

	FREE(buff);
	return 0;
}

static int
def_uid_handler(vector strvec)
{
	uid_t uid;
	char *buff;
	char passwd_buf[1024];
	struct passwd info, *found;

	buff = set_value(strvec);
	if (!buff)
		return 1;
	if (getpwnam_r(buff, &info, passwd_buf, 1024, &found) == 0 && found) {
		conf->attribute_flags |= (1 << ATTR_UID);
		conf->uid = info.pw_uid;
	}
	else if (sscanf(buff, "%u", &uid) == 1){
		conf->attribute_flags |= (1 << ATTR_UID);
		conf->uid = uid;
	}

	FREE(buff);
	return 0;
}

static int
def_gid_handler(vector strvec)
{
	gid_t gid;
	char *buff;
	char passwd_buf[1024];
	struct passwd info, *found;

	buff = set_value(strvec);
	if (!buff)
		return 1;

	if (getpwnam_r(buff, &info, passwd_buf, 1024, &found) == 0 && found) {
		conf->attribute_flags |= (1 << ATTR_GID);
		conf->gid = info.pw_gid;
	}
	else if (sscanf(buff, "%u", &gid) == 1){
		conf->attribute_flags |= (1 << ATTR_GID);
		conf->gid = gid;
	}
	FREE(buff);
	return 0;
}

static int
def_weight_handler(vector strvec)
{
	char * buff;

	buff = set_value(strvec);

	if (!buff)
		return 1;

	if (strlen(buff) == 10 &&
	    !strcmp(buff, "priorities"))
		conf->rr_weight = RR_WEIGHT_PRIO;

	FREE(buff);

	return 0;
}

static int
default_failback_handler(vector strvec)
{
	char * buff;

	buff = set_value(strvec);

	if (strlen(buff) == 6 && !strcmp(buff, "manual"))
		conf->pgfailback = -FAILBACK_MANUAL;
	else if (strlen(buff) == 9 && !strcmp(buff, "immediate"))
		conf->pgfailback = -FAILBACK_IMMEDIATE;
	else
		conf->pgfailback = atoi(buff);

	FREE(buff);

	return 0;
}

static int
def_no_path_retry_handler(vector strvec)
{
	char * buff;

	buff = set_value(strvec);
	if (!buff)
		return 1;

	if ((strlen(buff) == 4 && !strcmp(buff, "fail")) ||
	    (strlen(buff) == 1 && !strcmp(buff, "0")))
		conf->no_path_retry = NO_PATH_RETRY_FAIL;
	else if (strlen(buff) == 5 && !strcmp(buff, "queue"))
		conf->no_path_retry = NO_PATH_RETRY_QUEUE;
	else if ((conf->no_path_retry = atoi(buff)) < 1)
		conf->no_path_retry = NO_PATH_RETRY_UNDEF;

	FREE(buff);
	return 0;
}

static int
def_pg_timeout_handler(vector strvec)
{
	int pg_timeout;
	char * buff;

	buff = set_value(strvec);

	if (!buff)
		return 1;

	if (strlen(buff) == 4 && !strcmp(buff, "none"))
		conf->pg_timeout = -PGTIMEOUT_NONE;
	else if (sscanf(buff, "%d", &pg_timeout) == 1 && pg_timeout >= 0) {
		if (pg_timeout == 0)
			conf->pg_timeout = -PGTIMEOUT_NONE;
		else
			conf->pg_timeout = pg_timeout;
	}
	else
		conf->pg_timeout = PGTIMEOUT_UNDEF;

	FREE(buff);
	return 0;
}

static int
def_flush_on_last_del_handler(vector strvec)
{
	char * buff;

	buff = set_value(strvec);
	if (!buff)
		return 1;

	if ((strlen(buff) == 2 && strcmp(buff, "no") == 0) ||
	    (strlen(buff) == 1 && strcmp(buff, "0") == 0))
		conf->flush_on_last_del = FLUSH_DISABLED;
	if ((strlen(buff) == 3 && strcmp(buff, "yes") == 0) ||
	    (strlen(buff) == 1 && strcmp(buff, "1") == 0))
		conf->flush_on_last_del = FLUSH_ENABLED;
	else
		conf->flush_on_last_del = FLUSH_UNDEF;

	FREE(buff);
	return 0;
}

static int
names_handler(vector strvec)
{
	char * buff;

	buff = set_value(strvec);

	if (!buff)
		return 1;

	if ((strlen(buff) == 2 && !strcmp(buff, "no")) ||
	    (strlen(buff) == 1 && !strcmp(buff, "0")))
		conf->user_friendly_names = 0;
	else if ((strlen(buff) == 3 && !strcmp(buff, "yes")) ||
		 (strlen(buff) == 1 && !strcmp(buff, "1")))
		conf->user_friendly_names = 1;

	FREE(buff);
	return 0;
}

/*
 * blacklist block handlers
 */
static int
blacklist_handler(vector strvec)
{
	conf->blist_devnode = vector_alloc();
	conf->blist_wwid = vector_alloc();
	conf->blist_device = vector_alloc();

	if (!conf->blist_devnode || !conf->blist_wwid || !conf->blist_device)
		return 1;

	return 0;
}

static int
blacklist_exceptions_handler(vector strvec)
{
	conf->elist_devnode = vector_alloc();
	conf->elist_wwid = vector_alloc();
	conf->elist_device = vector_alloc();

	if (!conf->elist_devnode || !conf->elist_wwid || !conf->elist_device)
		return 1;

	return 0;
}

static int
ble_devnode_handler(vector strvec)
{
	char * buff;

	buff = set_value(strvec);

	if (!buff)
		return 1;

	return store_ble(conf->blist_devnode, buff, ORIGIN_CONFIG);
}

static int
ble_except_devnode_handler(vector strvec)
{
	char * buff;

	buff = set_value(strvec);

	if (!buff)
		return 1;

	return store_ble(conf->elist_devnode, buff, ORIGIN_CONFIG);
}

static int
ble_wwid_handler(vector strvec)
{
	char * buff;

	buff = set_value(strvec);

	if (!buff)
		return 1;

	return store_ble(conf->blist_wwid, buff, ORIGIN_CONFIG);
}

static int
ble_except_wwid_handler(vector strvec)
{
	char * buff;

	buff = set_value(strvec);

	if (!buff)
		return 1;

	return store_ble(conf->elist_wwid, buff, ORIGIN_CONFIG);
}

static int
ble_device_handler(vector strvec)
{
	return alloc_ble_device(conf->blist_device);
}

static int
ble_except_device_handler(vector strvec)
{
	return alloc_ble_device(conf->elist_device);
}

static int
ble_vendor_handler(vector strvec)
{
	char * buff;

	buff = set_value(strvec);

	if (!buff)
		return 1;

	return set_ble_device(conf->blist_device, buff, NULL, ORIGIN_CONFIG);
}

static int
ble_except_vendor_handler(vector strvec)
{
	char * buff;

	buff = set_value(strvec);

	if (!buff)
		return 1;

	return set_ble_device(conf->elist_device, buff, NULL, ORIGIN_CONFIG);
}

static int
ble_product_handler(vector strvec)
{
	char * buff;

	buff = set_value(strvec);

	if (!buff)
		return 1;

	return set_ble_device(conf->blist_device, NULL, buff, ORIGIN_CONFIG);
}

static int
ble_except_product_handler(vector strvec)
{
	char * buff;

	buff = set_value(strvec);

	if (!buff)
		return 1;

	return set_ble_device(conf->elist_device, NULL, buff, ORIGIN_CONFIG);
}

/*
 * devices block handlers
 */
static int
devices_handler(vector strvec)
{
	if (!conf->hwtable)
		conf->hwtable = vector_alloc();

	if (!conf->hwtable)
		return 1;

	return 0;
}

static int
device_handler(vector strvec)
{
	struct hwentry * hwe;

	hwe = alloc_hwe();

	if (!hwe)
		return 1;

	if (!vector_alloc_slot(conf->hwtable)) {
		free_hwe(hwe);
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
bl_product_handler(vector strvec)
{
	struct hwentry * hwe = VECTOR_LAST_SLOT(conf->hwtable);

	if (!hwe)
		return 1;

	hwe->bl_product = set_value(strvec);
	if (!hwe->bl_product)
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

	return 0;
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
	struct hwentry * hwe = VECTOR_LAST_SLOT(conf->hwtable);

	if (!hwe)
		return 1;

	hwe->checker_name = set_value(strvec);

	if (!hwe->checker_name)
		return 1;
	
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
hw_prio_handler(vector strvec)
{
	struct hwentry * hwe = VECTOR_LAST_SLOT(conf->hwtable);

	if (!hwe)
		return 1;

	hwe->prio_name = set_value(strvec);

	if (!hwe->prio_name)
		return 1;

	return 0;
}

static int
hw_failback_handler(vector strvec)
{
	struct hwentry * hwe = VECTOR_LAST_SLOT(conf->hwtable);
	char * buff;

	if (!hwe)
		return 1;

	buff = set_value(strvec);

	if (strlen(buff) == 6 && !strcmp(buff, "manual"))
		hwe->pgfailback = -FAILBACK_MANUAL;
	else if (strlen(buff) == 9 && !strcmp(buff, "immediate"))
		hwe->pgfailback = -FAILBACK_IMMEDIATE;
	else
		hwe->pgfailback = atoi(buff);

	FREE(buff);

	return 0;
}

static int
hw_weight_handler(vector strvec)
{
	struct hwentry * hwe = VECTOR_LAST_SLOT(conf->hwtable);
	char * buff;

	if (!hwe)
		return 1;

	buff = set_value(strvec);

	if (!buff)
		return 1;

	if (strlen(buff) == 10 &&
	    !strcmp(buff, "priorities"))
		hwe->rr_weight = RR_WEIGHT_PRIO;

	FREE(buff);

	return 0;
}

static int
hw_no_path_retry_handler(vector strvec)
{
	struct hwentry *hwe = VECTOR_LAST_SLOT(conf->hwtable);
	char *buff;

	if (!hwe)
		return 1;

	buff = set_value(strvec);
	if (!buff)
		return 1;

	if ((strlen(buff) == 4 && !strcmp(buff, "fail")) ||
	    (strlen(buff) == 1 && !strcmp(buff, "0")))
		hwe->no_path_retry = NO_PATH_RETRY_FAIL;
	else if (strlen(buff) == 5 && !strcmp(buff, "queue"))
		hwe->no_path_retry = NO_PATH_RETRY_QUEUE;
	else if ((hwe->no_path_retry = atoi(buff)) < 1)
		hwe->no_path_retry = NO_PATH_RETRY_UNDEF;

	FREE(buff);
	return 0;
}

static int
hw_minio_handler(vector strvec)
{
	struct hwentry *hwe = VECTOR_LAST_SLOT(conf->hwtable);
	char * buff;

	if (!hwe)
		return 1;

	buff = set_value(strvec);

	if (!buff)
		return 1;

	hwe->minio = atoi(buff);
	FREE(buff);

	return 0;
}

static int
hw_pg_timeout_handler(vector strvec)
{
	int pg_timeout;
	struct hwentry *hwe = VECTOR_LAST_SLOT(conf->hwtable);
	char *buff;

	if (!hwe)
		return 1;

	buff = set_value(strvec);

	if (!buff)
		return 1;

	if (strlen(buff) == 4 && !strcmp(buff, "none"))
		hwe->pg_timeout = -PGTIMEOUT_NONE;
	else if (sscanf(buff, "%d", &pg_timeout) == 1 && pg_timeout >= 0) {
		if (pg_timeout == 0)
			hwe->pg_timeout = -PGTIMEOUT_NONE;
		else
			hwe->pg_timeout = pg_timeout;
	}
	else
		hwe->pg_timeout = PGTIMEOUT_UNDEF;

	FREE(buff);
	return 0;
}

static int
hw_flush_on_last_del_handler(vector strvec)
{
	struct hwentry *hwe = VECTOR_LAST_SLOT(conf->hwtable);
	char * buff;

	if (!hwe)
		return 1;

	buff = set_value(strvec);
	if (!buff)
		return 1;

	if ((strlen(buff) == 2 && strcmp(buff, "no") == 0) ||
	    (strlen(buff) == 1 && strcmp(buff, "0") == 0))
		hwe->flush_on_last_del = FLUSH_DISABLED;
	if ((strlen(buff) == 3 && strcmp(buff, "yes") == 0) ||
	    (strlen(buff) == 1 && strcmp(buff, "1") == 0))
		hwe->flush_on_last_del = FLUSH_ENABLED;
	else
		hwe->flush_on_last_del = FLUSH_UNDEF;

	FREE(buff);
	return 0;
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

	mpe = alloc_mpe();

	if (!mpe)
		return 1;

	if (!vector_alloc_slot(conf->mptable)) {
		free_mpe(mpe);
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

static int
mp_failback_handler(vector strvec)
{
	struct mpentry * mpe = VECTOR_LAST_SLOT(conf->mptable);
	char * buff;

	if (!mpe)
		return 1;

	buff = set_value(strvec);

	if (strlen(buff) == 6 && !strcmp(buff, "manual"))
		mpe->pgfailback = -FAILBACK_MANUAL;
	else if (strlen(buff) == 9 && !strcmp(buff, "immediate"))
		mpe->pgfailback = -FAILBACK_IMMEDIATE;
	else
		mpe->pgfailback = atoi(buff);

	FREE(buff);

	return 0;
}

static int
mp_mode_handler(vector strvec)
{
	mode_t mode;
	struct mpentry *mpe = VECTOR_LAST_SLOT(conf->mptable);
	char *buff;

	if (!mpe)
		return 1;

	buff = set_value(strvec);
	if (!buff)
		return 1;
	if (sscanf(buff, "%o", &mode) == 1 && mode <= 0777) {
		mpe->attribute_flags |= (1 << ATTR_MODE);
		mpe->mode = mode;
	}

	FREE(buff);
	return 0;
}

static int
mp_uid_handler(vector strvec)
{
	uid_t uid;
	char *buff;
	char passwd_buf[1024];
	struct passwd info, *found;

	struct mpentry *mpe = VECTOR_LAST_SLOT(conf->mptable);

	if (!mpe)
		return 1;

	buff = set_value(strvec);
	if (!buff)
		return 1;

	if (getpwnam_r(buff, &info, passwd_buf, 1024, &found) == 0 && found) {
		mpe->attribute_flags |= (1 << ATTR_UID);
		mpe->uid = info.pw_uid;
	}
	else if (sscanf(buff, "%u", &uid) == 1){
		mpe->attribute_flags |= (1 << ATTR_UID);
		mpe->uid = uid;
	}
	FREE(buff);
	return 0;
}

static int
mp_gid_handler(vector strvec)
{
	gid_t gid;
	char *buff;
	char passwd_buf[1024];
	struct passwd info, *found;

	struct mpentry *mpe = VECTOR_LAST_SLOT(conf->mptable);

	if (!mpe)
		return 1;

	buff = set_value(strvec);
	if (!buff)
		return 1;

	if (getpwnam_r(buff, &info, passwd_buf, 1024, &found) == 0 && found) {
		mpe->attribute_flags |= (1 << ATTR_GID);
		mpe->gid = info.pw_gid;
	}
	else if (sscanf(buff, "%u", &gid) == 1) {
		mpe->attribute_flags |= (1 << ATTR_GID);
		mpe->gid = gid;
	}
	FREE(buff);
	return 0;
}

static int
mp_weight_handler(vector strvec)
{
	struct mpentry * mpe = VECTOR_LAST_SLOT(conf->mptable);
	char * buff;

	if (!mpe)
		return 1;

	buff = set_value(strvec);

	if (!buff)
		return 1;

	if (strlen(buff) == 10 &&
	    !strcmp(buff, "priorities"))
		mpe->rr_weight = RR_WEIGHT_PRIO;

	FREE(buff);

	return 0;
}

static int
mp_no_path_retry_handler(vector strvec)
{
	struct mpentry *mpe = VECTOR_LAST_SLOT(conf->mptable);
	char *buff;

	if (!mpe)
		return 1;

	buff = set_value(strvec);
	if (!buff)
		return 1;

	if ((strlen(buff) == 4 && !strcmp(buff, "fail")) ||
	    (strlen(buff) == 1 && !strcmp(buff, "0")))
		mpe->no_path_retry = NO_PATH_RETRY_FAIL;
	else if (strlen(buff) == 5 && !strcmp(buff, "queue"))
		mpe->no_path_retry = NO_PATH_RETRY_QUEUE;
	else if ((mpe->no_path_retry = atoi(buff)) < 1)
		mpe->no_path_retry = NO_PATH_RETRY_UNDEF;

	FREE(buff);
	return 0;
}

static int
mp_minio_handler(vector strvec)
{
	struct mpentry *mpe = VECTOR_LAST_SLOT(conf->mptable);
	char * buff;

	if (!mpe)
		return 1;

	buff = set_value(strvec);

	if (!buff)
		return 1;

	mpe->minio = atoi(buff);
	FREE(buff);

	return 0;
}

static int
mp_pg_timeout_handler(vector strvec)
{
	int pg_timeout;
	struct mpentry *mpe = VECTOR_LAST_SLOT(conf->mptable);
	char *buff;

	if (!mpe)
		return 1;

	buff = set_value(strvec);

	if (!buff)
		return 1;
	if (strlen(buff) == 4 && !strcmp(buff, "none"))
		mpe->pg_timeout = -PGTIMEOUT_NONE;
	else if (sscanf(buff, "%d", &pg_timeout) == 1 && pg_timeout >= 0) {
		if (pg_timeout == 0)
			mpe->pg_timeout = -PGTIMEOUT_NONE;
		else
			mpe->pg_timeout = pg_timeout;
	}
	else
		mpe->pg_timeout = PGTIMEOUT_UNDEF;

	FREE(buff);
	return 0;
}

static int
mp_flush_on_last_del_handler(vector strvec)
{
	struct mpentry *mpe = VECTOR_LAST_SLOT(conf->mptable);
	char * buff;

	if (!mpe)
		return 1;

	buff = set_value(strvec);
	if (!buff)
		return 1;

	if ((strlen(buff) == 2 && strcmp(buff, "no") == 0) ||
	    (strlen(buff) == 1 && strcmp(buff, "0") == 0))
		mpe->flush_on_last_del = FLUSH_DISABLED;
	if ((strlen(buff) == 3 && strcmp(buff, "yes") == 0) ||
	    (strlen(buff) == 1 && strcmp(buff, "1") == 0))
		mpe->flush_on_last_del = FLUSH_ENABLED;
	else
		mpe->flush_on_last_del = FLUSH_UNDEF;

	FREE(buff);
	return 0;
}

/*
 * config file keywords printing
 */
static int
snprint_mp_wwid (char * buff, int len, void * data)
{
	struct mpentry * mpe = (struct mpentry *)data;

	return snprintf(buff, len, "%s", mpe->wwid);
}

static int
snprint_mp_alias (char * buff, int len, void * data)
{
	struct mpentry * mpe = (struct mpentry *)data;

	if (!mpe->alias)
		return 0;

	if (conf->user_friendly_names &&
	    (strlen(mpe->alias) == strlen("mpath")) &&
	    !strcmp(mpe->alias, "mpath"))
		return 0;

	return snprintf(buff, len, "%s", mpe->alias);
}

static int
snprint_mp_path_grouping_policy (char * buff, int len, void * data)
{
	struct mpentry * mpe = (struct mpentry *)data;
	char str[POLICY_NAME_SIZE];

	if (!mpe->pgpolicy)
		return 0;
	get_pgpolicy_name(str, POLICY_NAME_SIZE, mpe->pgpolicy);

	return snprintf(buff, len, "%s", str);
}

static int
snprint_mp_selector (char * buff, int len, void * data)
{
	struct mpentry * mpe = (struct mpentry *)data;

	if (!mpe->selector)
		return 0;

	return snprintf(buff, len, "%s", mpe->selector);
}

static int
snprint_mp_failback (char * buff, int len, void * data)
{
	struct mpentry * mpe = (struct mpentry *)data;

	if (!mpe->pgfailback)
		return 0;

	switch(mpe->pgfailback) {
	case  FAILBACK_UNDEF:
		break;
	case -FAILBACK_MANUAL:
		return snprintf(buff, len, "manual");
	case -FAILBACK_IMMEDIATE:
		return snprintf(buff, len, "immediate");
	default:
		return snprintf(buff, len, "%i", mpe->pgfailback);
	}
	return 0;
}

static int
snprint_mp_mode(char * buff, int len, void * data)
{
	struct mpentry * mpe = (struct mpentry *)data;

	if ((mpe->attribute_flags & (1 << ATTR_MODE)) == 0)
		return 0;
	return snprintf(buff, len, "0%o", mpe->mode);
}

static int
snprint_mp_uid(char * buff, int len, void * data)
{
	struct mpentry * mpe = (struct mpentry *)data;

	if ((mpe->attribute_flags & (1 << ATTR_UID)) == 0)
		return 0;
	return snprintf(buff, len, "0%o", mpe->uid);
}

static int
snprint_mp_gid(char * buff, int len, void * data)
{
	struct mpentry * mpe = (struct mpentry *)data;

	if ((mpe->attribute_flags & (1 << ATTR_GID)) == 0)
		return 0;
	return snprintf(buff, len, "0%o", mpe->gid);
}

static int
snprint_mp_rr_weight (char * buff, int len, void * data)
{
	struct mpentry * mpe = (struct mpentry *)data;

	if (!mpe->rr_weight)
		return 0;
	if (mpe->rr_weight == RR_WEIGHT_PRIO)
		return snprintf(buff, len, "priorities");

	return 0;
}

static int
snprint_mp_no_path_retry (char * buff, int len, void * data)
{
	struct mpentry * mpe = (struct mpentry *)data;

	if (!mpe->no_path_retry)
		return 0;

	switch(mpe->no_path_retry) {
	case NO_PATH_RETRY_UNDEF:
		break;
	case NO_PATH_RETRY_FAIL:
		return snprintf(buff, len, "fail");
	case NO_PATH_RETRY_QUEUE:
		return snprintf(buff, len, "queue");
	default:
		return snprintf(buff, len, "%i",
				mpe->no_path_retry);
	}
	return 0;
}

static int
snprint_mp_rr_min_io (char * buff, int len, void * data)
{
	struct mpentry * mpe = (struct mpentry *)data;

	if (!mpe->minio)
		return 0;

	return snprintf(buff, len, "%u", mpe->minio);
}

static int
snprint_mp_pg_timeout (char * buff, int len, void * data)
{
	struct mpentry * mpe = (struct mpentry *)data;

	switch (mpe->pg_timeout) {
	case PGTIMEOUT_UNDEF:
		break;
	case -PGTIMEOUT_NONE:
		return snprintf(buff, len, "none");
	default:
		return snprintf(buff, len, "%i", mpe->pg_timeout);
	}
	return 0;
}

static int
snprint_mp_flush_on_last_del (char * buff, int len, void * data)
{
	struct mpentry * mpe = (struct mpentry *)data;

	switch (mpe->flush_on_last_del) {
	case FLUSH_DISABLED:
		return snprintf(buff, len, "no");
	case FLUSH_ENABLED:
		return snprintf(buff, len, "yes");
	}
	return 0;
}

static int
snprint_hw_vendor (char * buff, int len, void * data)
{
	struct hwentry * hwe = (struct hwentry *)data;

	if (!hwe->vendor)
		return 0;

	return snprintf(buff, len, "\"%s\"", hwe->vendor);
}

static int
snprint_hw_product (char * buff, int len, void * data)
{
	struct hwentry * hwe = (struct hwentry *)data;

	if (!hwe->product)
		return 0;

	return snprintf(buff, len, "\"%s\"", hwe->product);
}

static int
snprint_hw_bl_product (char * buff, int len, void * data)
{
	struct hwentry * hwe = (struct hwentry *)data;

	if (!hwe->bl_product)
		return 0;

	return snprintf(buff, len, "\"%s\"", hwe->bl_product);
}

static int
snprint_hw_getuid_callout (char * buff, int len, void * data)
{
	struct hwentry * hwe = (struct hwentry *)data;

	if (!hwe->getuid)
		return 0;
	if (strlen(hwe->getuid) == strlen(conf->getuid) &&
	    !strcmp(hwe->getuid, conf->getuid))
		return 0;

	return snprintf(buff, len, "\"%s\"", hwe->getuid);
}

static int
snprint_hw_prio (char * buff, int len, void * data)
{
	struct hwentry * hwe = (struct hwentry *)data;

	if (!hwe->prio_name)
		return 0;
	if (!strcmp(hwe->prio_name, conf->prio_name))
		return 0;
	
	return snprintf(buff, len, "%s", hwe->prio_name);
}

static int
snprint_hw_features (char * buff, int len, void * data)
{
	struct hwentry * hwe = (struct hwentry *)data;

	if (!hwe->features)
		return 0;
	if (strlen(hwe->features) == strlen(conf->features) &&
	    !strcmp(hwe->features, conf->features))
		return 0;

	return snprintf(buff, len, "\"%s\"", hwe->features);
}

static int
snprint_hw_hardware_handler (char * buff, int len, void * data)
{
	struct hwentry * hwe = (struct hwentry *)data;

	if (!hwe->hwhandler)
		return 0;
	if (strlen(hwe->hwhandler) == strlen(conf->hwhandler) &&
	    !strcmp(hwe->hwhandler, conf->hwhandler))
		return 0;

	return snprintf(buff, len, "\"%s\"", hwe->hwhandler);
}

static int
snprint_hw_selector (char * buff, int len, void * data)
{
	struct hwentry * hwe = (struct hwentry *)data;

	if (!hwe->selector)
		return 0;
	if (strlen(hwe->selector) == strlen(conf->selector) &&
	    !strcmp(hwe->selector, conf->selector))
		return 0;

	return snprintf(buff, len, "%s", hwe->selector);
}

static int
snprint_hw_path_grouping_policy (char * buff, int len, void * data)
{
	struct hwentry * hwe = (struct hwentry *)data;

	char str[POLICY_NAME_SIZE];

	if (!hwe->pgpolicy)
		return 0;
	if (hwe->pgpolicy == conf->pgpolicy)
		return 0;

	get_pgpolicy_name(str, POLICY_NAME_SIZE, hwe->pgpolicy);

	return snprintf(buff, len, "%s", str);
}

static int
snprint_hw_failback (char * buff, int len, void * data)
{
	struct hwentry * hwe = (struct hwentry *)data;

	if (!hwe->pgfailback)
		return 0;
	if (hwe->pgfailback == conf->pgfailback)
		return 0;

	switch(hwe->pgfailback) {
	case  FAILBACK_UNDEF:
		break;
	case -FAILBACK_MANUAL:
		return snprintf(buff, len, "manual");
	case -FAILBACK_IMMEDIATE:
		return snprintf(buff, len, "immediate");
	default:
		return snprintf(buff, len, "%i", hwe->pgfailback);
	}
	return 0;
}

static int
snprint_hw_rr_weight (char * buff, int len, void * data)
{
	struct hwentry * hwe = (struct hwentry *)data;

	if (!hwe->rr_weight)
		return 0;
	if (hwe->rr_weight == conf->rr_weight)
		return 0;
	if (hwe->rr_weight == RR_WEIGHT_PRIO)
		return snprintf(buff, len, "priorities");

	return 0;
}

static int
snprint_hw_no_path_retry (char * buff, int len, void * data)
{
	struct hwentry * hwe = (struct hwentry *)data;

	if (!hwe->no_path_retry)
		return 0;
	if (hwe->no_path_retry == conf->no_path_retry)
		return 0;

	switch(hwe->no_path_retry) {
	case NO_PATH_RETRY_UNDEF:
		break;
	case NO_PATH_RETRY_FAIL:
		return snprintf(buff, len, "fail");
	case NO_PATH_RETRY_QUEUE:
		return snprintf(buff, len, "queue");
	default:
		return snprintf(buff, len, "%i",
				hwe->no_path_retry);
	}
	return 0;
}

static int
snprint_hw_rr_min_io (char * buff, int len, void * data)
{
	struct hwentry * hwe = (struct hwentry *)data;

	if (!hwe->minio)
		return 0;
	if (hwe->minio == conf->minio)
		return 0;

	return snprintf(buff, len, "%u", hwe->minio);
}

static int
snprint_hw_pg_timeout (char * buff, int len, void * data)
{
	struct hwentry * hwe = (struct hwentry *)data;

	if (!hwe->pg_timeout)
		return 0;
	if (hwe->pg_timeout == conf->pg_timeout)
		return 0;

	switch (hwe->pg_timeout) {
	case PGTIMEOUT_UNDEF:
		break;
	case -PGTIMEOUT_NONE:
		return snprintf(buff, len, "none");
	default:
		return snprintf(buff, len, "%i", hwe->pg_timeout);
	}
	return 0;
}

static int
snprint_hw_flush_on_last_del (char * buff, int len, void * data)
{
	struct hwentry * hwe = (struct hwentry *)data;

	switch (hwe->flush_on_last_del) {
	case FLUSH_DISABLED:
		return snprintf(buff, len, "no");
	case FLUSH_ENABLED:
		return snprintf(buff, len, "yes");
	}
	return 0;
}

static int
snprint_hw_path_checker (char * buff, int len, void * data)
{
	struct hwentry * hwe = (struct hwentry *)data;

	if (!hwe->checker_name)
		return 0;
	if (!strcmp(hwe->checker_name, conf->checker_name))
		return 0;
	
	return snprintf(buff, len, "%s", hwe->checker_name);
}

static int
snprint_def_polling_interval (char * buff, int len, void * data)
{
	if (conf->checkint == DEFAULT_CHECKINT)
		return 0;
	return snprintf(buff, len, "%i", conf->checkint);
}

static int
snprint_def_verbosity (char * buff, int len, void * data)
{
	if (conf->checkint == DEFAULT_VERBOSITY)
		return 0;
	return snprintf(buff, len, "%i", conf->verbosity);
}

static int
snprint_def_udev_dir (char * buff, int len, void * data)
{
	if (!conf->udev_dir)
		return 0;
	if (strlen(DEFAULT_UDEVDIR) == strlen(conf->udev_dir) &&
	    !strcmp(conf->udev_dir, DEFAULT_UDEVDIR))
		return 0;

	return snprintf(buff, len, "\"%s\"", conf->udev_dir);
}

static int
snprint_def_multipath_dir (char * buff, int len, void * data)
{
	if (!conf->udev_dir)
		return 0;
	if (strlen(DEFAULT_MULTIPATHDIR) == strlen(conf->multipath_dir) &&
	    !strcmp(conf->multipath_dir, DEFAULT_MULTIPATHDIR))
		return 0;

	return snprintf(buff, len, "\"%s\"", conf->multipath_dir);
}

static int
snprint_def_selector (char * buff, int len, void * data)
{
	if (!conf->selector)
		return 0;
	if (strlen(conf->selector) == strlen(DEFAULT_SELECTOR) &&
	    !strcmp(conf->selector, DEFAULT_SELECTOR))
		return 0;

	return snprintf(buff, len, "%s", conf->selector);
}

static int
snprint_def_path_grouping_policy (char * buff, int len, void * data)
{
	char str[POLICY_NAME_SIZE];

	if (!conf->pgpolicy)
		return 0;
	if (conf->pgpolicy == DEFAULT_PGPOLICY)
		return 0;

	get_pgpolicy_name(str, POLICY_NAME_SIZE, conf->pgpolicy);

	return snprintf(buff, len, "%s", str);
}

static int
snprint_def_getuid_callout (char * buff, int len, void * data)
{
	if (!conf->getuid)
		return 0;
	if (strlen(conf->getuid) == strlen(DEFAULT_GETUID) &&
	    !strcmp(conf->getuid, DEFAULT_GETUID))
		return 0;

	return snprintf(buff, len, "\"%s\"", conf->getuid);
}

static int
snprint_def_prio (char * buff, int len, void * data)
{
	if (!conf->prio_name)
		return 0;

	if (strlen(conf->prio_name) == strlen(DEFAULT_PRIO) &&
	    !strcmp(conf->prio_name, DEFAULT_PRIO))
		return 0;
	
	return snprintf(buff, len, "%s", conf->prio_name);
}

static int
snprint_def_features (char * buff, int len, void * data)
{
	if (!conf->features)
		return 0;
	if (strlen(conf->features) == strlen(DEFAULT_FEATURES) &&
	    !strcmp(conf->features, DEFAULT_FEATURES))
		return 0;

	return snprintf(buff, len, "\"%s\"", conf->features);
}

static int
snprint_def_path_checker (char * buff, int len, void * data)
{
	if (!conf->checker_name)
		return 0;
	if (strlen(conf->checker_name) == strlen(DEFAULT_CHECKER) &&
	    !strcmp(conf->checker_name, DEFAULT_CHECKER))
		return 0;
	
	return snprintf(buff, len, "%s", conf->checker_name);
}

static int
snprint_def_failback (char * buff, int len, void * data)
{
	if (!conf->pgfailback)
		return 0;
	if (conf->pgfailback == DEFAULT_FAILBACK)
		return 0;

	switch(conf->pgfailback) {
	case  FAILBACK_UNDEF:
		break;
	case -FAILBACK_MANUAL:
		return snprintf(buff, len, "manual");
	case -FAILBACK_IMMEDIATE:
		return snprintf(buff, len, "immediate");
	default:
		return snprintf(buff, len, "%i", conf->pgfailback);
	}
	return 0;
}

static int
snprint_def_rr_min_io (char * buff, int len, void * data)
{
	if (!conf->minio)
		return 0;
	if (conf->minio == DEFAULT_MINIO)
		return 0;

	return snprintf(buff, len, "%u", conf->minio);
}

static int
snprint_max_fds (char * buff, int len, void * data)
{
	if (!conf->max_fds)
		return 0;

	return snprintf(buff, len, "%d", conf->max_fds);
}

static int
snprint_def_mode(char * buff, int len, void * data)
{
	if ((conf->attribute_flags & (1 << ATTR_MODE)) == 0)
		return 0;
	return snprintf(buff, len, "0%o", conf->mode);
}

static int
snprint_def_uid(char * buff, int len, void * data)
{
	if ((conf->attribute_flags & (1 << ATTR_UID)) == 0)
		return 0;
	return snprintf(buff, len, "0%o", conf->uid);
}

static int
snprint_def_gid(char * buff, int len, void * data)
{
	if ((conf->attribute_flags & (1 << ATTR_GID)) == 0)
		return 0;
	return snprintf(buff, len, "0%o", conf->gid);
}

static int
snprint_def_rr_weight (char * buff, int len, void * data)
{
	if (!conf->rr_weight)
		return 0;
	if (conf->rr_weight == DEFAULT_RR_WEIGHT)
		return 0;
	if (conf->rr_weight == RR_WEIGHT_PRIO)
		return snprintf(buff, len, "priorities");

	return 0;
}

static int
snprint_def_no_path_retry (char * buff, int len, void * data)
{
	if (conf->no_path_retry == DEFAULT_NO_PATH_RETRY)
		return 0;

	switch(conf->no_path_retry) {
	case NO_PATH_RETRY_UNDEF:
		break;
	case NO_PATH_RETRY_FAIL:
		return snprintf(buff, len, "fail");
	case NO_PATH_RETRY_QUEUE:
		return snprintf(buff, len, "queue");
	default:
		return snprintf(buff, len, "%i",
				conf->no_path_retry);
	}
	return 0;
}

static int
snprint_def_pg_timeout (char * buff, int len, void * data)
{
	if (conf->pg_timeout == DEFAULT_PGTIMEOUT)
		return 0;

	switch (conf->pg_timeout) {
	case PGTIMEOUT_UNDEF:
		break;
	case -PGTIMEOUT_NONE:
		return snprintf(buff, len, "none");
	default:
		return snprintf(buff, len, "%i", conf->pg_timeout);
	}
	return 0;
}

static int
snprint_def_flush_on_last_del (char * buff, int len, void * data)
{
	switch (conf->flush_on_last_del) {
	case FLUSH_DISABLED:
		return snprintf(buff, len, "no");
	case FLUSH_ENABLED:
		return snprintf(buff, len, "yes");
	}
	return 0;
}

static int
snprint_def_user_friendly_names (char * buff, int len, void * data)
{
	if (conf->user_friendly_names == DEFAULT_USER_FRIENDLY_NAMES)
		return 0;
	if (!conf->user_friendly_names)
		return snprintf(buff, len, "no");

	return snprintf(buff, len, "yes");
}

static int
snprint_ble_simple (char * buff, int len, void * data)
{
	struct blentry * ble = (struct blentry *)data;

	return snprintf(buff, len, "\"%s\"", ble->str);
}

static int
snprint_bled_vendor (char * buff, int len, void * data)
{
	struct blentry_device * bled = (struct blentry_device *)data;

	return snprintf(buff, len, "\"%s\"", bled->vendor);
}

static int
snprint_bled_product (char * buff, int len, void * data)
{
	struct blentry_device * bled = (struct blentry_device *)data;

	return snprintf(buff, len, "\"%s\"", bled->product);
}

#define __deprecated

void
init_keywords(void)
{
	install_keyword_root("defaults", NULL);
	install_keyword("verbosity", &verbosity_handler, &snprint_def_verbosity);
	install_keyword("polling_interval", &polling_interval_handler, &snprint_def_polling_interval);
	install_keyword("udev_dir", &udev_dir_handler, &snprint_def_udev_dir);
	install_keyword("multipath_dir", &multipath_dir_handler, &snprint_def_multipath_dir);
	install_keyword("selector", &def_selector_handler, &snprint_def_selector);
	install_keyword("path_grouping_policy", &def_pgpolicy_handler, &snprint_def_path_grouping_policy);
	install_keyword("getuid_callout", &def_getuid_callout_handler, &snprint_def_getuid_callout);
	install_keyword("prio", &def_prio_handler, &snprint_def_prio);
	install_keyword("features", &def_features_handler, &snprint_def_features);
	install_keyword("path_checker", &def_path_checker_handler, &snprint_def_path_checker);
	install_keyword("checker", &def_path_checker_handler, &snprint_def_path_checker);
	install_keyword("failback", &default_failback_handler, &snprint_def_failback);
	install_keyword("rr_min_io", &def_minio_handler, &snprint_def_rr_min_io);
	install_keyword("max_fds", &max_fds_handler, &snprint_max_fds);
	install_keyword("rr_weight", &def_weight_handler, &snprint_def_rr_weight);
	install_keyword("no_path_retry", &def_no_path_retry_handler, &snprint_def_no_path_retry);
	install_keyword("pg_timeout", &def_pg_timeout_handler, &snprint_def_pg_timeout);
	install_keyword("flush_on_last_del", &def_flush_on_last_del_handler, &snprint_def_flush_on_last_del);
	install_keyword("user_friendly_names", &names_handler, &snprint_def_user_friendly_names);
	install_keyword("mode", &def_mode_handler, &snprint_def_mode);
	install_keyword("uid", &def_uid_handler, &snprint_def_uid);
	install_keyword("gid", &def_gid_handler, &snprint_def_gid);
	__deprecated install_keyword("default_selector", &def_selector_handler, NULL);
	__deprecated install_keyword("default_path_grouping_policy", &def_pgpolicy_handler, NULL);
	__deprecated install_keyword("default_getuid_callout", &def_getuid_callout_handler, NULL);
	__deprecated install_keyword("default_features", &def_features_handler, NULL);
	__deprecated install_keyword("default_path_checker", &def_path_checker_handler, NULL);

	install_keyword_root("blacklist", &blacklist_handler);
	install_keyword("devnode", &ble_devnode_handler, &snprint_ble_simple);
	install_keyword("wwid", &ble_wwid_handler, &snprint_ble_simple);
	install_keyword("device", &ble_device_handler, NULL);
	install_sublevel();
	install_keyword("vendor", &ble_vendor_handler, &snprint_bled_vendor);
	install_keyword("product", &ble_product_handler, &snprint_bled_product);
	install_sublevel_end();
	install_keyword_root("blacklist_exceptions", &blacklist_exceptions_handler);
	install_keyword("devnode", &ble_except_devnode_handler, &snprint_ble_simple);
	install_keyword("wwid", &ble_except_wwid_handler, &snprint_ble_simple);
	install_keyword("device", &ble_except_device_handler, NULL);
	install_sublevel();
	install_keyword("vendor", &ble_except_vendor_handler, &snprint_bled_vendor);
	install_keyword("product", &ble_except_product_handler, &snprint_bled_product);
	install_sublevel_end();

#if 0
	__deprecated install_keyword_root("devnode_blacklist", &blacklist_handler);
	__deprecated install_keyword("devnode", &ble_devnode_handler, &snprint_ble_simple);
	__deprecated install_keyword("wwid", &ble_wwid_handler, &snprint_ble_simple);
	__deprecated install_keyword("device", &ble_device_handler, NULL);
	__deprecated install_sublevel();
	__deprecated install_keyword("vendor", &ble_vendor_handler, &snprint_bled_vendor);
	__deprecated install_keyword("product", &ble_product_handler, &snprint_bled_product);
	__deprecated install_sublevel_end();
#endif

	install_keyword_root("devices", &devices_handler);
	install_keyword("device", &device_handler, NULL);
	install_sublevel();
	install_keyword("vendor", &vendor_handler, &snprint_hw_vendor);
	install_keyword("product", &product_handler, &snprint_hw_product);
	install_keyword("product_blacklist", &bl_product_handler, &snprint_hw_bl_product);
	install_keyword("path_grouping_policy", &hw_pgpolicy_handler, &snprint_hw_path_grouping_policy);
	install_keyword("getuid_callout", &hw_getuid_callout_handler, &snprint_hw_getuid_callout);
	install_keyword("path_selector", &hw_selector_handler, &snprint_hw_selector);
	install_keyword("path_checker", &hw_path_checker_handler, &snprint_hw_path_checker);
	install_keyword("checker", &hw_path_checker_handler, &snprint_hw_path_checker);
	install_keyword("features", &hw_features_handler, &snprint_hw_features);
	install_keyword("hardware_handler", &hw_handler_handler, &snprint_hw_hardware_handler);
	install_keyword("prio", &hw_prio_handler, &snprint_hw_prio);
	install_keyword("failback", &hw_failback_handler, &snprint_hw_failback);
	install_keyword("rr_weight", &hw_weight_handler, &snprint_hw_rr_weight);
	install_keyword("no_path_retry", &hw_no_path_retry_handler, &snprint_hw_no_path_retry);
	install_keyword("rr_min_io", &hw_minio_handler, &snprint_hw_rr_min_io);
	install_keyword("pg_timeout", &hw_pg_timeout_handler, &snprint_hw_pg_timeout);
	install_keyword("flush_on_last_del", &hw_flush_on_last_del_handler, &snprint_hw_flush_on_last_del);
	install_sublevel_end();

	install_keyword_root("multipaths", &multipaths_handler);
	install_keyword("multipath", &multipath_handler, NULL);
	install_sublevel();
	install_keyword("wwid", &wwid_handler, &snprint_mp_wwid);
	install_keyword("alias", &alias_handler, &snprint_mp_alias);
	install_keyword("path_grouping_policy", &mp_pgpolicy_handler, &snprint_mp_path_grouping_policy);
	install_keyword("path_selector", &mp_selector_handler, &snprint_mp_selector);
	install_keyword("failback", &mp_failback_handler, &snprint_mp_failback);
	install_keyword("rr_weight", &mp_weight_handler, &snprint_mp_rr_weight);
	install_keyword("no_path_retry", &mp_no_path_retry_handler, &snprint_mp_no_path_retry);
	install_keyword("rr_min_io", &mp_minio_handler, &snprint_mp_rr_min_io);
	install_keyword("pg_timeout", &mp_pg_timeout_handler, &snprint_mp_pg_timeout);
	install_keyword("flush_on_last_del", &mp_flush_on_last_del_handler, &snprint_mp_flush_on_last_del);
	install_keyword("mode", &mp_mode_handler, &snprint_mp_mode);
	install_keyword("uid", &mp_uid_handler, &snprint_mp_uid);
	install_keyword("gid", &mp_gid_handler, &snprint_mp_gid);
	install_sublevel_end();
}
