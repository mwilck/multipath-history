#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "util.h"
#include "vector.h"
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

struct config *
alloc_config (void)
{
	return zalloc(sizeof(struct config));
}

void
free_config (struct config * conf)
{
	free(conf->dev);
	free(conf->multipath);
	free(conf->udev_dir);
	free(conf->default_selector);
	free(conf->default_getuid);
	free(conf->default_getprio);
	free(conf->default_features);
	free(conf->default_hwhandler);

	vector_free(conf->mptable);
	vector_free(conf->hwtable);
	vector_free(conf->aliases);
	vector_free(conf->blist);
	vector_free(conf->binvec);

	free(conf);
}
	
