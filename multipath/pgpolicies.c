#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "main.h"
#include "global.h"
#include "memory.h"
#include "debug.h"

#define PATH_STR_SIZE	16

/*
 * Transforms the path group vector into a proper device map string
 */
void
assemble_map (struct multipath * mp)
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

	/*
	 * select the right path selector :
	 * 1) set internal default
	 * 2) override by controler wide settings
	 * 3) override by LUN specific settings
	 */

	/* 1) set internal default */
	selector = conf->default_selector;
	selector_args = conf->default_selector_args;

	/* 2) override by controler wide settings */
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

	/* 3) override by LUN specific settings */
	for (i = 0; i < VECTOR_SIZE(conf->mptable); i++) {
		mpe = VECTOR_SLOT(conf->mptable, i);

		if (strcmp(mpe->wwid, mp->wwid) == 0) {
			selector = (mpe->selector) ?
				   mpe->selector : conf->default_selector;
			selector_args = (mpe->selector_args) ?
				   mpe->selector_args : conf->default_selector_args;
		}
	}

	p = mp->params;
	p += sprintf(p, " %i", VECTOR_SIZE(mp->pg));

	for (i = 0; i < VECTOR_SIZE(mp->pg); i++) {
		pgpaths = VECTOR_SLOT(mp->pg, i);
		p += sprintf(p, " %s %i %i",
			     selector, VECTOR_SIZE(pgpaths), selector_args);

		for (j = 0; j < VECTOR_SIZE(pgpaths); j++)
			p += sprintf(p, " %s",
				     (char *)VECTOR_SLOT(pgpaths, j));
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
		strncpy(pathstr, pp->dev_t, strlen(pp->dev_t) - 1);
		vector_alloc_slot(pgpaths);
		vector_set_slot(pgpaths, pathstr);
				
		bitmap[i] = 1;

		for (k = i + 1; k < VECTOR_SIZE(mp->paths); k++) {
			
			if (bitmap[k])
				continue;

			pp2 = VECTOR_SLOT(mp->paths, k);
			
			if (0 == strcmp(pp->serial, pp2->serial)) {
				pathstr = zalloc(PATH_STR_SIZE);
				strncpy(pathstr, pp2->dev_t,
					strlen(pp2->dev_t) - 1);
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
		strncpy(pathstr, pp->dev_t, strlen(pp->dev_t) - 1);
		vector_alloc_slot(pgpaths);
		vector_set_slot(pgpaths, pathstr);
				
		bitmap[i] = 1;

		for (k = i - 1; k >= 0; k--) {
			
			if (bitmap[k])
				continue;

			pp2 = VECTOR_SLOT(mp->paths, k);
			
			if (0 == strcmp(pp->serial, pp2->serial)) {
				pathstr = zalloc(PATH_STR_SIZE);
				strncpy(pathstr, pp2->dev_t,
					strlen(pp2->dev_t) - 1);
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

	mp->pg = vector_alloc();
	
	for (i = 0; i < VECTOR_SIZE(mp->paths); i++) {
		pp = VECTOR_SLOT(mp->paths, i);

		if (0 != pp->sg_id.scsi_type)
			continue;

		if (!pp->tur) {
			pathstr = zalloc(PATH_STR_SIZE);
			strncpy(pathstr, pp->dev_t, strlen(pp->dev_t) - 1);
			pgpaths = vector_alloc();
			vector_alloc_slot(pgpaths);
			vector_set_slot(pgpaths, pathstr);
			vector_alloc_slot(mp->pg);
			vector_set_slot(mp->pg, pgpaths);
		} else {
			pathstr = zalloc(PATH_STR_SIZE);
			strncpy(pathstr, pp->dev_t, strlen(pp->dev_t) - 1);
			pgpaths = vector_alloc();
			vector_alloc_slot(pgpaths);
			vector_set_slot(pgpaths, pathstr);
			vector_alloc_slot(mp->pg);
			vector_set_slot(mp->pg, pgpaths);
		}
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

		if (!pp->tur) {
			pathstr = zalloc(PATH_STR_SIZE);
			strncpy(pathstr, pp->dev_t, strlen(pp->dev_t) - 1);
			vector_alloc_slot(pgfailedpaths);
			vector_set_slot(pgfailedpaths, pathstr);
		} else {
			pathstr = zalloc(PATH_STR_SIZE);
			strncpy(pathstr, pp->dev_t, strlen(pp->dev_t) - 1);
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
