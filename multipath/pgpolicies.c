/*
 * Here we define the path grouping policies
 */
#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <path_state.h>
#include <safe_printf.h>

#include <memory.h>
#include "main.h"
#include "pgpolicies.h"

extern int
get_pgpolicy_id (char * str)
{
	if (0 == strncmp(str, "failover", 8))
		return FAILOVER;
	if (0 == strncmp(str, "multibus", 8))
		return MULTIBUS;
	if (0 == strncmp(str, "group_by_serial", 15))
		return GROUP_BY_SERIAL;
	if (0 == strncmp(str, "group_by_prio", 13))
		return GROUP_BY_PRIO;

	return -1;
}

extern void
get_pgpolicy_name (char * buff, int id)
{
	char * s;

	switch (id) {
	case FAILOVER:
		s = "failover";
		break;
	case MULTIBUS:
		s = "multibus";
		break;
	case GROUP_BY_SERIAL:
		s = "group_by_serial";
		break;
	case GROUP_BY_PRIO:
		s = "group_by_prio";
		break;
	default:
		s = "undefined";
		break;
	}
	if(safe_snprintf(buff, POLICY_NAME_SIZE, "%s", s)) {
		fprintf(stderr, "get_pgpolicy_name: buff too small\n");
		exit(1);
	}
}

extern void
group_by_status (struct multipath * mp, int state)
{
	int i;
	struct path * pp;
	struct pathgroup * pgp;
	vector failedpaths;
	vector pathsleft;

	if (mp->pg == NULL)
		mp->pg = vector_alloc();

	failedpaths = vector_alloc();
	pathsleft = vector_alloc();

	for (i = 0; i < VECTOR_SIZE(mp->paths); i++) {
		pp = VECTOR_SLOT(mp->paths, i);

		if (pp->state == state) {
			vector_alloc_slot(failedpaths);
			vector_set_slot(failedpaths, pp);
		} else {
			vector_alloc_slot(pathsleft);
			vector_set_slot(pathsleft, pp);
		}
	}
	if (VECTOR_SIZE(failedpaths) > 0) {
		pgp = zalloc(sizeof(struct pathgroup));
		pgp->paths = failedpaths;
		vector_alloc_slot(mp->pg);
		vector_set_slot(mp->pg, pgp);
		vector_free(mp->paths);
		mp->paths = pathsleft;
	} else {
		vector_free(pathsleft);
		vector_free(failedpaths);
	}
	return;
}

/*
 * One path group per unique serial number present in the path vector
 */
extern void
group_by_serial (struct multipath * mp) {
	int i, j;
	int * bitmap;
	struct path * pp;
	struct pathgroup * pgp;
	struct path * pp2;
	
	if (mp->pg == NULL)
		mp->pg = vector_alloc();

	/* init the bitmap */
	bitmap = zalloc(VECTOR_SIZE(mp->paths) * sizeof (int));

	for (i = 0; i < VECTOR_SIZE(mp->paths); i++) {

		if (bitmap[i])
			continue;

		pp = VECTOR_SLOT(mp->paths, i);

		/* here, we really got a new pg */
		pgp = zalloc(sizeof(struct pathgroup));
		pgp->paths = vector_alloc();
		vector_alloc_slot(mp->pg);
		vector_set_slot(mp->pg, pgp);

		/* feed the first path */
		vector_alloc_slot(pgp->paths);
		vector_set_slot(pgp->paths, pp);
				
		bitmap[i] = 1;

		for (j = i + 1; j < VECTOR_SIZE(mp->paths); j++) {
			
			if (bitmap[j])
				continue;

			pp2 = VECTOR_SLOT(mp->paths, j);
			
			if (0 == strcmp(pp->serial, pp2->serial)) {
				vector_alloc_slot(pgp->paths);
				vector_set_slot(pgp->paths, pp2);
				bitmap[j] = 1;
			}
		}
	}
	free(bitmap);
	vector_free(mp->paths);
	mp->paths = NULL;
}

extern void
one_path_per_group (struct multipath * mp)
{
	int i;
	struct path * pp;
	struct pathgroup * pgp;

	if (mp->pg == NULL)
		mp->pg = vector_alloc();
	
	for (i = 0; i < VECTOR_SIZE(mp->paths); i++) {
		pp = VECTOR_SLOT(mp->paths, i);
		pgp = zalloc(sizeof(struct pathgroup));
		pgp->paths = vector_alloc();
		vector_alloc_slot(pgp->paths);
		vector_set_slot(pgp->paths, pp);
		vector_alloc_slot(mp->pg);
		vector_set_slot(mp->pg, pgp);
	}
	vector_free(mp->paths);
	mp->paths = NULL;
}

extern void
one_group (struct multipath * mp)	/* aka multibus */
{
	int i;
	struct path * pp;
	struct pathgroup * pgp;

	pgp = zalloc(sizeof(struct pathgroup));
	pgp->paths = vector_alloc();

	if (mp->pg == NULL)
		mp->pg = vector_alloc();

	for (i = 0; i < VECTOR_SIZE(mp->paths); i++) {
		pp = VECTOR_SLOT(mp->paths, i);
		vector_alloc_slot(pgp->paths);
		vector_set_slot(pgp->paths, pp);
	}
	if (VECTOR_SIZE(pgp->paths) > 0) {
		vector_alloc_slot(mp->pg);
		vector_set_slot(mp->pg, pgp);
	} else {
		vector_free(pgp->paths);
		free(pgp);
	}
	vector_free(mp->paths);
	mp->paths = NULL;
	return;
}

extern void
group_by_prio (struct multipath * mp)
{
	int i;
	unsigned int prio;
	struct path * pp;
	struct pathgroup * pgp;

	if (mp->pg == NULL)
		mp->pg = vector_alloc();

	while (VECTOR_SIZE(mp->paths) > 0) {
		/*
		 * init a new pgpaths, put in the first path in mp->paths
		 */
		pp = VECTOR_SLOT(mp->paths, 0);
		prio = pp->priority;
		pgp = zalloc(sizeof(struct pathgroup));
		pgp->paths = vector_alloc();
		vector_alloc_slot(pgp->paths);
		vector_set_slot(pgp->paths, pp);
		vector_alloc_slot(mp->pg);
		vector_set_slot(mp->pg, pgp);
		vector_del_slot(mp->paths, 0);
		
		/*
		 * add the other paths with the same prio
		 */
		for (i = 0; i < VECTOR_SIZE(mp->paths); i++) {
			pp = VECTOR_SLOT(mp->paths, i);
			if (pp->priority == prio) {
				vector_alloc_slot(pgp->paths);
				vector_set_slot(pgp->paths, pp);
				vector_del_slot(mp->paths, i);
			}
		}
	}
	vector_free(mp->paths);
	mp->paths = NULL;
}
