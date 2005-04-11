#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "util.h"
#include "vector.h"
#include "blacklist.h"
#include "config.h"

struct hwentry *
find_hwe (vector hwtable, char * vendor, char * product)
{
	int i;
	struct hwentry * hwe;

	vector_foreach_slot (hwtable, hwe, i) {
		if (strcmp_chomp(hwe->vendor, vendor) == 0 &&
		    (hwe->product[0] == '*' ||
		    strcmp_chomp(hwe->product, product) == 0))
			return hwe;
	}
	return NULL;
}

extern struct mpentry *
find_mpe (char * wwid)
{
	int i;
	struct mpentry * mpe;

	if (!wwid)
		return NULL;

	vector_foreach_slot (conf->mptable, mpe, i)
		if (mpe->wwid && strcmp(mpe->wwid, wwid) == 0)
			return mpe;

	return NULL;
}

extern char *
get_mpe_wwid (char * alias)
{
	int i;
	struct mpentry * mpe;

	if (!alias)
		return NULL;

	vector_foreach_slot (conf->mptable, mpe, i)
		if (mpe->alias && strcmp(mpe->alias, alias) == 0)
			return mpe->wwid;

	return NULL;
}

void
free_hwe (struct hwentry * hwe)
{
	if (!hwe)
		return;

	if (hwe->vendor)
		FREE(hwe->vendor);

	if (hwe->product)
		FREE(hwe->product);

	if (hwe->selector)
		FREE(hwe->selector);

	if (hwe->getuid)
		FREE(hwe->getuid);

	if (hwe->getprio)
		FREE(hwe->getprio);

	if (hwe->features)
		FREE(hwe->features);

	if (hwe->hwhandler)
		FREE(hwe->hwhandler);

	FREE(hwe);
}

void
free_hwtable (vector hwtable)
{
	int i;
	struct hwentry * hwe;

	if (!hwtable)
		return;

	vector_foreach_slot (hwtable, hwe, i)
		free_hwe(hwe);

	vector_free(hwtable);
}

void
free_mpe (struct mpentry * mpe)
{
	if (!mpe)
		return;

	if (mpe->wwid)
		FREE(mpe->wwid);

	if (mpe->selector)
		FREE(mpe->selector);

	if (mpe->getuid)
		FREE(mpe->getuid);

	if (mpe->alias)
		FREE(mpe->alias);

	FREE(mpe);
}

void
free_mptable (vector mptable)
{
	int i;
	struct mpentry * mpe;

	if (!mptable)
		return;

	vector_foreach_slot (mptable, mpe, i)
		free_mpe(mpe);

	vector_free(mptable);
}

struct config *
alloc_config (void)
{
	return (struct config *)MALLOC(sizeof(struct config));
}

void
free_config (struct config * conf)
{
	if (!conf)
		return;

	if (conf->dev)
		FREE(conf->dev);

	if (conf->multipath)
		FREE(conf->multipath);

	if (conf->udev_dir)
		FREE(conf->udev_dir);

	if (conf->default_selector)
		FREE(conf->default_selector);

	if (conf->default_getuid)
		FREE(conf->default_getuid);

	if (conf->default_getprio)
		FREE(conf->default_getprio);

	if (conf->default_features)
		FREE(conf->default_features);

	if (conf->default_hwhandler)
		FREE(conf->default_hwhandler);

	free_blacklist(conf->blist);
	free_mptable(conf->mptable);
	free_hwtable(conf->hwtable);
	free_strvec(conf->binvec);

	FREE(conf);
}
	
