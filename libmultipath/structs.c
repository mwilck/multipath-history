#include "memory.h"
#include "vector.h"
#include "util.h"
#include "structs.h"

struct path *
alloc_path (void)
{
	return zalloc(sizeof(struct path));
}


struct multipath *
alloc_multipath (void)
{
	return zalloc(sizeof(struct multipath));
}

void
free_multipath (struct multipath * mpp)
{
	struct pathgroup * pgp;
	int i;

	if (mpp->paths)
		vector_free(mpp->paths);

	vector_foreach_slot (mpp->pg, pgp, i)
		if (pgp->paths)
			vector_free(pgp->paths);
	free(mpp);

	return;
}

struct multipath *
find_mp (vector mp, char * alias)
{
	int i;
	int len;
	struct multipath * mpp;
	
	len = strlen(alias);

	if (!len)
		return NULL;
	
	vector_foreach_slot (mp, mpp, i) {
		if (strlen(mpp->alias) == len &&
		    !strncmp(mpp->alias, alias, len))
			return mpp;
	}
	return NULL;
}

struct path *
find_path_by_dev (vector pathvec, char * dev)
{
	int i;
	struct path * pp;
	
	vector_foreach_slot (pathvec, pp, i)
		if (!strcmp_chomp(pp->dev, dev))
			return pp;

//	dbg("path %s not found in pathvec\n", dev);
	return NULL;
}

struct path *
find_path_by_devt (vector pathvec, char * dev_t)
{
	int i;
	struct path * pp;

	vector_foreach_slot (pathvec, pp, i)
		if (!strcmp_chomp(pp->dev_t, dev_t))
			return pp;

//	dbg("path %s not found in pathvec\n", dev_t);
	return NULL;
}

