#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "main.h"
#include "memory.h"
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

/*
 * One path group per unique serial number present in the path vector
 * Simple rotation logic for the head pg's serial
 */
extern void
group_by_serial (struct multipath * mp, int slot) {
	int i, k;
	int * bitmap;
	struct path * pp;
	struct path * pp2;
	vector pgpaths;
	char * pathstr;
	
	mp->pg = vector_alloc();

	/* init the bitmap */
	bitmap = zalloc(VECTOR_SIZE(mp->paths) * sizeof (int));

	if (slot % 2)
		goto even;
	
	/* scan paths bottom up */
	for (i = 0; i < VECTOR_SIZE(mp->paths); i++) {
		if (bitmap[i])
			continue;

		pp = VECTOR_SLOT(mp->paths, i);

		/* here, we really got a new pg */
		pgpaths = vector_alloc();
		vector_alloc_slot(mp->pg);
		vector_set_slot(mp->pg, pgpaths);

		/* feed the first path */
		pathstr = zalloc(PATH_STR_SIZE);
		if(safe_snprintf(pathstr, PATH_STR_SIZE, "%s", pp->dev_t)) {
			fprintf(stderr, "group_by_serial: pathstr too small\n");
			exit(1);
		}
		vector_alloc_slot(pgpaths);
		vector_set_slot(pgpaths, pathstr);
				
		bitmap[i] = 1;

		for (k = i + 1; k < VECTOR_SIZE(mp->paths); k++) {
			
			if (bitmap[k])
				continue;

			pp2 = VECTOR_SLOT(mp->paths, k);
			
			if (0 == strcmp(pp->serial, pp2->serial)) {
				pathstr = zalloc(PATH_STR_SIZE);
				if(safe_snprintf(pathstr, PATH_STR_SIZE, "%s",
						 pp2->dev_t)) {
					fprintf(stderr, "group_by_serial: "
							"pathstr too small\n");
					exit(1);
				}
				vector_alloc_slot(pgpaths);
				vector_set_slot(pgpaths, pathstr);

				bitmap[k] = 1;
			}
		}
	}

even:
	/* scan paths top down */
	for (i = VECTOR_SIZE(mp->paths) - 1; i >= 0; i--) {
		if (bitmap[i])
			continue;

		pp = VECTOR_SLOT(mp->paths, i);

		/* here, we really got a new pg */
		pgpaths = vector_alloc();
		vector_alloc_slot(mp->pg);
		vector_set_slot(mp->pg, pgpaths);

		/* feed the first path */
		pathstr = zalloc(PATH_STR_SIZE);
		if(safe_snprintf(pathstr, PATH_STR_SIZE, "%s", pp->dev_t)) {
			fprintf(stderr, "group_by_serial: pathstr too small\n");
			exit(1);
		}
		vector_alloc_slot(pgpaths);
		vector_set_slot(pgpaths, pathstr);
				
		bitmap[i] = 1;

		for (k = i - 1; k >= 0; k--) {
			
			if (bitmap[k])
				continue;

			pp2 = VECTOR_SLOT(mp->paths, k);
			
			if (0 == strcmp(pp->serial, pp2->serial)) {
				pathstr = zalloc(PATH_STR_SIZE);
				if(safe_snprintf(pathstr, PATH_STR_SIZE, "%s",
						 pp2->dev_t)) {
					fprintf(stderr, "group_by_serial: "
							"pathstr too small\n");
					exit(1);
				}
				vector_alloc_slot(pgpaths);
				vector_set_slot(pgpaths, pathstr);

				bitmap[k] = 1;
			}
		}
	}
	free(bitmap);
}

extern void
one_path_per_group (struct multipath * mp)
{
	int i;
	char * pathstr;
	struct path * pp;
	vector pgpaths;
	vector failedpaths;

	mp->pg = vector_alloc();
	failedpaths = vector_alloc();
	
	for (i = 0; i < VECTOR_SIZE(mp->paths); i++) {
		pp = VECTOR_SLOT(mp->paths, i);
		if (0 != pp->sg_id.scsi_type)
			continue;

		pathstr = zalloc(PATH_STR_SIZE);
		if(safe_snprintf(pathstr, PATH_STR_SIZE, "%s", pp->dev_t)) {
			fprintf(stderr, "one_path_per_group: "
					"pathstr too small\n");
			exit(1);
		}
		if (!pp->state) {
			vector_alloc_slot(failedpaths);
			vector_set_slot(failedpaths, pathstr);
		} else {
			pgpaths = vector_alloc();
			vector_alloc_slot(pgpaths);
			vector_set_slot(pgpaths, pathstr);
			vector_alloc_slot(mp->pg);
			vector_set_slot(mp->pg, pgpaths);
		}
	}
	if (VECTOR_SIZE(failedpaths) > 0) {
		vector_alloc_slot(mp->pg);
		vector_set_slot(mp->pg, failedpaths);
	}
}

extern void
one_group (struct multipath * mp)
{
	int i;
	char * pathstr;
	struct path * pp;
	vector pgvalidpaths;
	vector pgfailedpaths;

	pgvalidpaths = vector_alloc();
	pgfailedpaths = vector_alloc();
	mp->pg = vector_alloc();

	for (i = 0; i < VECTOR_SIZE(mp->paths); i++) {
		pp = VECTOR_SLOT(mp->paths, i);

		if (0 != pp->sg_id.scsi_type)
			continue;

		pathstr = zalloc(PATH_STR_SIZE);
		if(safe_snprintf(pathstr, PATH_STR_SIZE, "%s", pp->dev_t)) {
			fprintf(stderr, "one_group: pathstr too small\n");
			exit(1);
		}
		if (!pp->state) {
			vector_alloc_slot(pgfailedpaths);
			vector_set_slot(pgfailedpaths, pathstr);
		} else {
			vector_alloc_slot(pgvalidpaths);
			vector_set_slot(pgvalidpaths, pathstr);
		}
	}
	if (VECTOR_SIZE(pgvalidpaths) > 0) {
		vector_alloc_slot(mp->pg);
		vector_set_slot(mp->pg, pgvalidpaths);
	}
	if (VECTOR_SIZE(pgfailedpaths) > 0) {
		vector_alloc_slot(mp->pg);
		vector_set_slot(mp->pg, pgfailedpaths);
	}
}

extern void
group_by_prio (struct multipath * mp)
{
	int i;
	unsigned int prio = -1;
	char * pathstr;
	struct path * pp;
	vector pgpaths = NULL;
	vector pgfailedpaths;

	mp->pg = vector_alloc();
	pgfailedpaths = vector_alloc();

	for (i = 0; i < VECTOR_SIZE(mp->paths); i++) {
		pp = VECTOR_SLOT(mp->paths, i);

		if (0 != pp->sg_id.scsi_type)
			continue;

		pathstr = zalloc(PATH_STR_SIZE);
		if(safe_snprintf(pathstr, PATH_STR_SIZE, "%s", pp->dev_t)) {
			fprintf(stderr, "group_by_prio: pathstr too small\n");
			exit(1);
		}
		if (!pp->state) {
			vector_alloc_slot(pgfailedpaths);
			vector_set_slot(pgfailedpaths, pathstr);
		} else {
			if (pp->priority != prio || pgpaths == NULL) {
				pgpaths = vector_alloc();
				vector_alloc_slot(mp->pg);
				vector_set_slot(mp->pg, pgpaths);
				prio = pp->priority;
			}
			vector_alloc_slot(pgpaths);
			vector_set_slot(pgpaths, pathstr);
		}
	}
	if (VECTOR_SIZE(pgfailedpaths) > 0) {
		vector_alloc_slot(mp->pg);
		vector_set_slot(mp->pg, pgfailedpaths);
	}
}
