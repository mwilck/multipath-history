#include <stdio.h>
#include <unistd.h>

#include "memory.h"
#include "vector.h"
#include "util.h"
#include "structs.h"
#include "debug.h"

struct path *
alloc_path (void)
{
	return zalloc(sizeof(struct path));
}

void
free_path (struct path * pp)
{
	if (pp && pp->fd > 0)
		close(pp->fd);

	free(pp);
}

void
free_pathvec (vector vec, int free_paths)
{
	int i;
	struct path * pp;

	if (free_paths && vec && VECTOR_SIZE(vec) > 0)
		vector_foreach_slot(vec, pp, i)
			free_path(pp);

	vector_free(vec);
}

struct pathgroup *
alloc_pathgroup (void)
{
	struct pathgroup * pgp;

	pgp = zalloc(sizeof(struct pathgroup));

	if (!pgp)
		return NULL;

	pgp->paths = vector_alloc();

	if (!pgp->paths)
		free(pgp);

	return pgp;
}

void
free_pathgroup (struct pathgroup * pgp, int free_paths)
{
	if (pgp && pgp->paths)
		free_pathvec(pgp->paths, free_paths);

	free(pgp);
}

void
free_pgvec (vector pgvec, int free_paths)
{
	int i;
	struct pathgroup * pgp;

	vector_foreach_slot(pgvec, pgp, i)
		free_pathgroup(pgp, free_paths);

	vector_free(pgvec);
}

struct multipath *
alloc_multipath (void)
{
	return zalloc(sizeof(struct multipath));
}

void
free_multipath (struct multipath * mpp, int free_paths)
{
	if (mpp->paths)
		free_pathvec(mpp->paths, free_paths);

	if (mpp->pg)
		free_pgvec(mpp->pg, free_paths);
	free(mpp);
}

int
store_path (vector pathvec, struct path * pp)
{
	if (!vector_alloc_slot(pathvec))
		return 1;

	vector_set_slot(pathvec, pp);

	return 0;
}

int
store_pathgroup (vector pgvec, struct pathgroup * pgp)
{
	if (!vector_alloc_slot(pgvec))
		return 1;

	vector_set_slot(pgvec, pgp);

	return 0;
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

	condlog(3, "path %s not found in pathvec\n", dev);
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

	condlog(3, "path %s not found in pathvec\n", dev_t);
	return NULL;
}

