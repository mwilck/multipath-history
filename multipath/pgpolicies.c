/*
 * Here we define the path grouping policies
 *
 * These policies are of several  type :
 * 	- intermediate or layerable policies
 * 	  ex: group_by_status
 * 	  these match certain paths in the multipath and group them
 * 	  leaving non-matching paths to following policies
 * 	  
 * 	- leaf policies
 * 	  ex: multibus | failover | group_by_serial | group_by_prio
 * 	  these take all available paths left and group them
 * 	  according to their policy, leaving no paths to subsequent
 * 	  policies
 * 	  
 * 	- path groups sorter
 * 	  ex: sort_pg_by_summed_prio
 * 	  these are used to reorder paths groups according to their
 * 	  policy. they are executed after path grouping proper. I
 * 	  guess it's clear only one can be used at a time.
 *
 * At least one leaf policy must be traversed, other are optional
 *
 */
#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <path_state.h>

#include <memory.h>
#include "main.h"
#include "pgpolicies.h"
#include "debug.h"

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
		vector_alloc_slot(mp->pg);
		vector_set_slot(mp->pg, failedpaths);
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
 * Simple rotation logic for the head pg's serial
 */
extern void
group_by_serial (struct multipath * mp) {
	int i, k;
	int * bitmap;
	struct path * pp;
	struct path * pp2;
	vector pgpaths;
	
	if (mp->pg == NULL)
		mp->pg = vector_alloc();

	/* init the bitmap */
	bitmap = zalloc(VECTOR_SIZE(mp->paths) * sizeof (int));

	for (i = 0; i < VECTOR_SIZE(mp->paths); i++) {
		if (bitmap[i])
			continue;

		pp = VECTOR_SLOT(mp->paths, i);

		/* here, we really got a new pg */
		pgpaths = vector_alloc();
		vector_alloc_slot(mp->pg);
		vector_set_slot(mp->pg, pgpaths);

		/* feed the first path */
		vector_alloc_slot(pgpaths);
		vector_set_slot(pgpaths, pp);
				
		bitmap[i] = 1;

		for (k = i + 1; k < VECTOR_SIZE(mp->paths); k++) {
			
			if (bitmap[k])
				continue;

			pp2 = VECTOR_SLOT(mp->paths, k);
			
			if (0 == strcmp(pp->serial, pp2->serial)) {
				vector_alloc_slot(pgpaths);
				vector_set_slot(pgpaths, pp);
				bitmap[k] = 1;
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
	vector pgpaths;

	if (mp->pg == NULL)
		mp->pg = vector_alloc();
	
	for (i = 0; i < VECTOR_SIZE(mp->paths); i++) {
		pp = VECTOR_SLOT(mp->paths, i);
		pgpaths = vector_alloc();
		vector_alloc_slot(pgpaths);
		vector_set_slot(pgpaths, pp);
		vector_alloc_slot(mp->pg);
		vector_set_slot(mp->pg, pgpaths);
	}
	vector_free(mp->paths);
	mp->paths = NULL;
}

extern void
one_group (struct multipath * mp)	/* aka multibus */
{
	int i;
	struct path * pp;
	vector pgpaths;

	pgpaths = vector_alloc();

	if (mp->pg == NULL)
		mp->pg = vector_alloc();

	for (i = 0; i < VECTOR_SIZE(mp->paths); i++) {
		pp = VECTOR_SLOT(mp->paths, i);
		vector_alloc_slot(pgpaths);
		vector_set_slot(pgpaths, pp);
	}
	if (VECTOR_SIZE(pgpaths) > 0) {
		vector_alloc_slot(mp->pg);
		vector_set_slot(mp->pg, pgpaths);
	} else {
		vector_free(pgpaths);
	}
	vector_free(mp->paths);
	mp->paths = NULL;
	return;
}

extern void
group_by_prio (struct multipath * mp)
{
	int i;
	unsigned int prio = -1;
	struct path * pp;
	vector pgpaths = NULL;

	if (mp->pg == NULL)
		mp->pg = vector_alloc();

	for (i = 0; i < VECTOR_SIZE(mp->paths); i++) {
		pp = VECTOR_SLOT(mp->paths, i);
		if (pp->priority != prio || pgpaths == NULL) {
			pgpaths = vector_alloc();
			vector_alloc_slot(mp->pg);
			vector_set_slot(mp->pg, pgpaths);
			prio = pp->priority;
		}
		vector_alloc_slot(pgpaths);
		vector_set_slot(pgpaths, pp);
	}
	vector_free(mp->paths);
	mp->paths = NULL;
}

extern void
sort_pg_by_summed_prio (struct multipath * mp)
{
	int i, j, k;
	int sum = 0, ref_sum = 0;
	vector sortedpg;
	vector pgpaths, ref_pgpaths;
	struct path * pp;
	
	if (mp->pg == NULL)
		return;
	
	if (VECTOR_SIZE(mp->pg) < 2)
		return;

	/*
	 * fill first slot of the ordered vector
	 */
	pgpaths = VECTOR_SLOT(mp->pg, 0);

	sortedpg = vector_alloc();
	vector_alloc_slot(sortedpg);
	vector_set_slot(sortedpg, pgpaths);
	
	for (i = 1; i < VECTOR_SIZE(mp->pg); i++) {
		ref_pgpaths = VECTOR_SLOT(mp->pg, i);
		for (j = 0; j < VECTOR_SIZE(ref_pgpaths); j++) {
			pp = VECTOR_SLOT(ref_pgpaths, j);
			if (pp->state != PATH_DOWN)
				ref_sum += pp->priority;
		}
		for (j = 0; j < VECTOR_SIZE(sortedpg); j++) {
			pgpaths = VECTOR_SLOT(sortedpg, j);
			for (k = 0; k < VECTOR_SIZE(pgpaths); k++) {
				pp = VECTOR_SLOT(pgpaths, k);
				if (pp->state != PATH_DOWN)
					sum += pp->priority;
			}
			if (sum < ref_sum) {
				vector_insert_slot(sortedpg, j, ref_pgpaths);
				break;
			}
		}
		if (j == VECTOR_SIZE(sortedpg)) {
			vector_alloc_slot(sortedpg);
			vector_set_slot(sortedpg, ref_pgpaths);
		}
	}
	vector_free(mp->pg);
	mp->pg = sortedpg;
}
