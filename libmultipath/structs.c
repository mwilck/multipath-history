#include "memory.h"
#include "vector.h"
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
	struct pathgroup * pgp;
	
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
