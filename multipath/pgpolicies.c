#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "main.h"
#include "pgpolicies.h"
#include "vector.h"
#include "memory.h"

#define SELECTOR	"round-robin"
#define SELECTOR_ARGS	0

extern void
group_by_tur (struct multipath * mp, char * str) {
	int left_path_count = 0;
	int right_path_count = 0;
	int i;
	char left_path_buff[FILE_NAME_SIZE];
	char right_path_buff[FILE_NAME_SIZE];
	char * left_path_buff_p = &left_path_buff[0];
	char * right_path_buff_p = &right_path_buff[0];
	struct path * pp;

	for (i = 0; i < VECTOR_SIZE(mp->paths); i++) {
		pp = VECTOR_SLOT(mp->paths, i);

		if (pp->tur) {
			
			left_path_buff_p += sprintf (left_path_buff_p, " %s",
						pp->dev);
			
			left_path_count++;

		} else {

			right_path_buff_p += sprintf (right_path_buff_p, " %s",
						pp->dev);

			right_path_count++;
		}
	}

	if (!left_path_count)
		sprintf (str, " 1 " SELECTOR " %i %i%s",
			 right_path_count, SELECTOR_ARGS, right_path_buff);

	else if (!right_path_count)
		sprintf (str, " 1 " SELECTOR " %i %i%s",
			 left_path_count, SELECTOR_ARGS, left_path_buff);

	else
		sprintf (str, " 2 " SELECTOR " %i %i%s " \
				    SELECTOR " %i %i%s",
			left_path_count, SELECTOR_ARGS, left_path_buff,
			right_path_count, SELECTOR_ARGS, right_path_buff);
}

extern void
group_by_serial (struct multipath * mp, int slot, char * str) {
	int path_count, pg_count = 0;
	int i, k;
	int * bitmap;
	char path_buff[FILE_NAME_SIZE];
	char pg_buff[FILE_NAME_SIZE];
	char * path_buff_p = &path_buff[0];
	char * pg_buff_p = &pg_buff[0];
	struct path * pp;
	struct path * pp2;

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
		pg_count++;
		path_count = 1;
		memset (&path_buff, 0, FILE_NAME_SIZE * sizeof (char));
		path_buff_p = &path_buff[0];

		path_buff_p += sprintf (path_buff_p, " %s", pp->dev);
		bitmap[i] = 1;

		for (k = i + 1; k < VECTOR_SIZE(mp->paths); k++) {
			
			if (bitmap[k])
				continue;

			pp2 = VECTOR_SLOT(mp->paths, k);
			
			if (0 == strcmp (pp->serial, pp2->serial)) {
				path_buff_p += sprintf (path_buff_p, " %s",
							pp2->dev);

				bitmap[k] = 1;
				path_count++;
			}
		}

		pg_buff_p += sprintf (pg_buff_p,
				      " " SELECTOR " %i %i%s",
				      path_count, SELECTOR_ARGS, path_buff);

	}

even:
	/* scan paths top down */
	for (i = VECTOR_SIZE(mp->paths) - 1; i >= 0; i--) {
		if (bitmap[i])
			continue;

		pp = VECTOR_SLOT(mp->paths, i);

		/* here, we really got a new pg */
		pg_count++;
		path_count = 1;
		memset (&path_buff, 0, FILE_NAME_SIZE * sizeof (char));
		path_buff_p = &path_buff[0];

		path_buff_p += sprintf (path_buff_p, " %s", pp->dev);
		bitmap[i] = 1;

		for (k = i - 1; k >= 0; k--) {
			
			if (bitmap[k])
				continue;

			pp2 = VECTOR_SLOT(mp->paths, k);
			
			if (0 == strcmp (pp->serial, pp2->serial)) {
				path_buff_p += sprintf (path_buff_p, " %s",
							pp2->dev);

				bitmap[k] = 1;
				path_count++;
			}
		}

		pg_buff_p += sprintf (pg_buff_p,
				      " " SELECTOR " %i %i%s",
				      path_count, SELECTOR_ARGS, path_buff);
	}


	sprintf (str, " %i%s", pg_count, pg_buff);
	free (bitmap);
}


extern void
one_path_per_group (struct multipath * mp, char * str)
{
	int i;
	char * p;
	struct path * pp;

	p = str;

	p += sprintf (p, " %i", VECTOR_SIZE(mp->paths));

	for (i=0; i < VECTOR_SIZE(mp->paths); i++) {
		pp = VECTOR_SLOT(mp->paths, i);

		if (0 != pp->sg_id.scsi_type)
			continue;

		p += sprintf (p, " " SELECTOR);
		p += sprintf (p, " 1 %i", SELECTOR_ARGS);
		p += sprintf (p, " %s", pp->dev);
	}
}

extern void
one_group (struct multipath * mp, char * str)
{
	int i, np = 0;
	char * p;
	struct path * pp = NULL;

	p = str;

	for (i=0; i < VECTOR_SIZE(mp->paths); i++) {
		pp = VECTOR_SLOT(mp->paths, i);
		if (0 == pp->sg_id.scsi_type)
		np++;
	}
	
	p += sprintf (p, " 1 " SELECTOR " %i %i", np, SELECTOR_ARGS);
	
	for (i = 0; i < VECTOR_SIZE(mp->paths); i++) {

		if (0 != pp->sg_id.scsi_type)
			continue;

		pp = VECTOR_SLOT(mp->paths, i);
		p += sprintf (p, " %s", pp->dev);
	}
}
