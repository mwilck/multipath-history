#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "main.h"
#include "global.h"
#include "vector.h"
#include "memory.h"

#define PATH_STR_SIZE	16

/*
 * Transforms the path group vector into a proper device map string
 */
static void
assemble_map (struct multipath * mp, char * str, vector pg)
{
	int i, j;
	char * p;
	vector pgpaths;
	char * selector;
	int selector_args;
	struct mpentry * mpe;
	struct hwentry * hwe;
	struct path * pp;

	pp = VECTOR_SLOT(mp->paths, 0);

	/* universal settings */
	selector = conf->default_selector;
	selector_args = conf->default_selector_args;

	/* override by controler wide settings */
	for (i = 0; i < VECTOR_SIZE(conf->hwtable); i++) {
		hwe = VECTOR_SLOT(conf->hwtable, i);

		if (strcmp(hwe->vendor, pp->vendor_id) == 0 &&
		    strcmp(hwe->product, pp->product_id) == 0) {
			selector = (hwe->selector) ?
				   hwe->selector : conf->default_selector;
			selector_args = (hwe->selector_args) ?
				   hwe->selector_args : conf->default_selector_args;
		}
	}

	/* override by LUN specific settings */
	for (i = 0; i < VECTOR_SIZE(conf->mptable); i++) {
		mpe = VECTOR_SLOT(conf->mptable, i);

		if (strcmp(mpe->wwid, mp->wwid) == 0) {
			selector = (mpe->selector) ?
				   mpe->selector : conf->default_selector;
			selector_args = (mpe->selector_args) ?
				   mpe->selector_args : conf->default_selector_args;
		}
	}

	p = str;
	p += sprintf (p, " %i", VECTOR_SIZE(pg));

	for (i = 0; i < VECTOR_SIZE(pg); i++) {
		pgpaths = VECTOR_SLOT(pg, i);
		p += sprintf (p, " %s %i %i", selector, VECTOR_SIZE(pgpaths), selector_args);

		for (j = 0; j < VECTOR_SIZE(pgpaths); j++)
			p += sprintf (p, " %s",
					(char *)VECTOR_SLOT(pgpaths, j));
	}
}

/*
 * Two paths groups at most : one for paths with TUR==OK, one for the other
 * Needed for controlers that present ghost paths
 */
extern void
group_by_tur (struct multipath * mp, char * str) {
	int i;
	struct path * pp;
	vector pg;
	vector pgpaths_left;
	vector pgpaths_right;
	char * pathstr;

	pgpaths_left = vector_alloc();
	pgpaths_right = vector_alloc();

	for (i = 0; i < VECTOR_SIZE(mp->paths); i++) {
		pp = VECTOR_SLOT(mp->paths, i);
		pathstr = zalloc (PATH_STR_SIZE);
		strncpy (pathstr, pp->dev, strlen(pp->dev));

		if (pp->tur) {
			vector_alloc_slot (pgpaths_left);
			vector_set_slot (pgpaths_left, pathstr);
		} else {
			vector_alloc_slot (pgpaths_right);
			vector_set_slot (pgpaths_right, pathstr);
		}
	}
	pg = vector_alloc();
	vector_alloc_slot (pg);

	if (!VECTOR_SIZE(pgpaths_left))
		vector_set_slot (pg, pgpaths_right);

	else if (!VECTOR_SIZE(pgpaths_right))
		vector_set_slot (pg, pgpaths_left);

	else {
		vector_set_slot (pg, pgpaths_left);
		vector_alloc_slot (pg);
		vector_set_slot (pg, pgpaths_right);
	}
	assemble_map (mp, str, pg);
}

/*
 * One path group per unique serial number present in the path vector
 * Simple rotation logic for the head pg's serial
 */
extern void
group_by_serial (struct multipath * mp, int slot, char * str) {
	int i, k;
	int * bitmap;
	struct path * pp;
	struct path * pp2;
	vector pg;
	vector pgpaths;
	char * pathstr;
	
	pg = vector_alloc();

	/* init the bitmap */
	bitmap = zalloc (VECTOR_SIZE(mp->paths) * sizeof (int));

	if (slot % 2)
		goto even;
	
	/* scan paths bottom up */
	for (i = 0; i < VECTOR_SIZE(mp->paths); i++) {
		if (bitmap[i])
			continue;

		pp = VECTOR_SLOT(mp->paths, i);

		/* here, we really got a new pg */
		pgpaths = vector_alloc();
		vector_alloc_slot (pg);
		vector_set_slot (pg, pgpaths);

		/* feed the first path */
		pathstr = zalloc (PATH_STR_SIZE);
		strncpy (pathstr, pp->dev, strlen(pp->dev));
		vector_alloc_slot (pgpaths);
		vector_set_slot (pgpaths, pathstr);
				
		bitmap[i] = 1;

		for (k = i + 1; k < VECTOR_SIZE(mp->paths); k++) {
			
			if (bitmap[k])
				continue;

			pp2 = VECTOR_SLOT(mp->paths, k);
			
			if (0 == strcmp (pp->serial, pp2->serial)) {
				pathstr = zalloc (PATH_STR_SIZE);
				strncpy (pathstr, pp2->dev, strlen(pp2->dev));
				vector_alloc_slot (pgpaths);
				vector_set_slot (pgpaths, pathstr);

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
		vector_alloc_slot (pg);
		vector_set_slot (pg, pgpaths);

		/* feed the first path */
		pathstr = zalloc (PATH_STR_SIZE);
		strncpy (pathstr, pp->dev, strlen(pp->dev));
		vector_alloc_slot (pgpaths);
		vector_set_slot (pgpaths, pathstr);
				
		bitmap[i] = 1;

		for (k = i - 1; k >= 0; k--) {
			
			if (bitmap[k])
				continue;

			pp2 = VECTOR_SLOT(mp->paths, k);
			
			if (0 == strcmp (pp->serial, pp2->serial)) {
				pathstr = zalloc (PATH_STR_SIZE);
				strncpy (pathstr, pp2->dev, strlen(pp2->dev));
				vector_alloc_slot (pgpaths);
				vector_set_slot (pgpaths, pathstr);

				bitmap[k] = 1;
			}
		}
	}
	free (bitmap);
	assemble_map (mp, str, pg);
}

/*
 * This is for pure failover
 */
extern void
one_path_per_group (struct multipath * mp, char * str)
{
	int i;
	char * pathstr;
	struct path * pp;
	vector pg;
	vector pgpaths;

	pg = vector_alloc();
	
	for (i=0; i < VECTOR_SIZE(mp->paths); i++) {
		pp = VECTOR_SLOT(mp->paths, i);

		if (0 != pp->sg_id.scsi_type)
			continue;

		pathstr = zalloc (PATH_STR_SIZE);
		strncpy (pathstr, pp->dev, strlen(pp->dev));
		pgpaths = vector_alloc();
		vector_alloc_slot (pgpaths);
		vector_set_slot (pgpaths, pathstr);
		vector_alloc_slot (pg);
		vector_set_slot (pg, pgpaths);
	}
	assemble_map (mp, str, pg);
}

/*
 * This is for symmetric array controlers, with no switch penalty
 */
extern void
one_group (struct multipath * mp, char * str)
{
	int i;
	char * pathstr;
	struct path * pp;
	vector pg;
	vector pgpaths;

	pgpaths = vector_alloc();
	pg = vector_alloc();
	vector_alloc_slot (pg);
	vector_set_slot (pg, pgpaths);

	for (i = 0; i < VECTOR_SIZE(mp->paths); i++) {
		pp = VECTOR_SLOT(mp->paths, i);

		if (0 != pp->sg_id.scsi_type)
			continue;

		pathstr = zalloc (PATH_STR_SIZE);
		strncpy (pathstr, pp->dev, strlen(pp->dev));
		vector_alloc_slot (pgpaths);
		vector_set_slot (pgpaths, pathstr);
	}
	assemble_map (mp, str, pg);
}
