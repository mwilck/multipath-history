#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "main.h"
#include "pgpolicies.h"
#include "vector.h"

#define SELECTOR	"round-robin"
#define SELECTOR_ARGS	0

extern void
group_by_tur (struct multipath * mp, vector pathvec, char * str) {
	int left_path_count = 0;
	int right_path_count = 0;
	int i;
	char left_path_buff[FILE_NAME_SIZE];
	char right_path_buff[FILE_NAME_SIZE];
	char * left_path_buff_p = &left_path_buff[0];
	char * right_path_buff_p = &right_path_buff[0];
	struct path * pp;

	for (i = 0; i <= mp->npaths; i++) {
		pp = VECTOR_SLOT(pathvec, mp->pindex[i]);

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
group_by_serial (struct multipath * mp, vector pathvec, char * str) {
	int path_count, pg_count = 0;
	int i, k;
	int * bitmap;
	char path_buff[FILE_NAME_SIZE];
	char pg_buff[FILE_NAME_SIZE];
	char * path_buff_p = &path_buff[0];
	char * pg_buff_p = &pg_buff[0];
	struct path * pp;

	/* init the bitmap */
	bitmap = malloc ((mp->npaths + 1) * sizeof (int));
	memset (bitmap, 0, (mp->npaths + 1) * sizeof (int));

	for (i = 0; i <= mp->npaths; i++) {
		if (bitmap[i])
			continue;

		pp = VECTOR_SLOT(pathvec, mp->pindex[i]);

		/* here, we really got a new pg */
		pg_count++;
		path_count = 1;
		memset (&path_buff, 0, FILE_NAME_SIZE * sizeof (char));
		path_buff_p = &path_buff[0];

		path_buff_p += sprintf (path_buff_p, " %s", pp->dev);
		bitmap[i] = 1;

		for (k = i + 1; k <= mp->npaths; k++) {
			
			if (bitmap[k])
				continue;

			if (0 == strcmp (pp->serial, pp->serial)) {
				path_buff_p += sprintf (path_buff_p, " %s",
							pp->dev);

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
one_path_per_group (struct multipath * mp, vector pathvec, char * str)
{
	int i;
	char * p;
	struct path * pp;

	p = str;

	p += sprintf (p, " %i", mp->npaths + 1);

	for (i=0; i <= mp->npaths; i++) {
		pp = VECTOR_SLOT(pathvec, mp->pindex[i]);

		if (0 != pp->sg_id.scsi_type)
			continue;

		p += sprintf (p, " " SELECTOR);
		p += sprintf (p, " 1 %i", SELECTOR_ARGS);
		p += sprintf (p, " %s", pp->dev);
	}
}

extern void
one_group (struct multipath * mp, vector pathvec, char * str)
{
	int i, np = 0;
	char * p;
	struct path * pp = NULL;

	p = str;

	for (i=0; i <= mp->npaths; i++) {
		pp = VECTOR_SLOT(pathvec, mp->pindex[i]);
		if (0 == pp->sg_id.scsi_type)
		np++;
	}
	
	p += sprintf (p, " 1 " SELECTOR " %i %i", np, SELECTOR_ARGS);
	
	for (i=0; i<= mp->npaths; i++) {

		if (0 != pp->sg_id.scsi_type)
			continue;

		p += sprintf (p, " %s", pp->dev);
	}
}
