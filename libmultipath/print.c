#include <stdio.h>
#include <string.h>
#include <math.h>

#include "vector.h"
#include "structs.h"
#include "print.h"
#include "dmparser.h"

#include "../libcheckers/path_state.h"

#define MAX_FIELD_LEN 64

#define MAX(x,y) (x > y) ? x : y

void
get_path_layout (struct path_layout * pl, vector pathvec)
{
	int i;
	char buff[MAX_FIELD_LEN];
	struct path * pp;

	int uuid_len;
	int hbtl_len;
	int dev_len;
	int dev_t_len;
	int prio_len;

	/* reset max col lengths */
	pl->uuid_len = 0;
	pl->hbtl_len = 0;
	pl->dev_len = 0;
	pl->dev_t_len = 0;
	pl->prio_len = 0;

	vector_foreach_slot (pathvec, pp, i) {
		uuid_len = strlen(pp->wwid);
		hbtl_len = snprintf(buff, MAX_FIELD_LEN, "%i:%i:%i:%i",
					pp->sg_id.host_no,
					pp->sg_id.channel,
					pp->sg_id.scsi_id,
					pp->sg_id.lun);
		dev_len = strlen(pp->dev);
		dev_t_len = strlen(pp->dev_t);
		prio_len = 1 + (int)log10(pp->priority);

		pl->uuid_len = MAX(uuid_len, pl->uuid_len);
		pl->hbtl_len = MAX(hbtl_len, pl->hbtl_len);
		pl->dev_len = MAX(dev_len, pl->dev_len);
		pl->dev_t_len = MAX(dev_t_len, pl->dev_t_len);
		pl->prio_len = MAX(prio_len, pl->prio_len);
	}
	return;
}

void
get_map_layout (struct map_layout * ml, vector mpvec)
{
	int i;
	char buff[MAX_FIELD_LEN];
	struct multipath * mpp;

	int mapname_len;
	int mapdev_len;
	int failback_progress_len;
	int queueing_progress_len;
	int nr_active_len;

	/* reset max col lengths */
	ml->mapname_len = 0;
	ml->mapdev_len = 0;
	ml->failback_progress_len = 0;
	ml->queueing_progress_len = 0;
	ml->nr_active_len = 0;

	vector_foreach_slot (mpvec, mpp, i) {
		mapname_len = (mpp->alias) ?
				strlen(mpp->alias) : strlen(mpp->wwid);
		mapdev_len = snprintf(buff, MAX_FIELD_LEN,
			       	      "dm-%i", mpp->minor);
		failback_progress_len = 4 + PROGRESS_LEN +
					(int)log10(mpp->failback_tick) +
					(int)log10(mpp->pgfailback);
		queueing_progress_len = 5 + (int)log10(mpp->retry_tick);
		nr_active_len = (int)log10(mpp->nr_active);

		ml->mapname_len = MAX(mapname_len, ml->mapname_len);
		ml->mapdev_len = MAX(mapdev_len, ml->mapdev_len);
		ml->failback_progress_len = MAX(failback_progress_len,
						ml->failback_progress_len);
		ml->queueing_progress_len = MAX(queueing_progress_len,
						ml->queueing_progress_len);
		ml->nr_active_len = MAX(nr_active_len, ml->nr_active_len);
	}
	return;
}

#define TAIL   (line + len - 1 - c)
#define PAD(x) while ((int)(c - s) < (x) && (c < (line + len - 1))) \
			*c++ = ' '; s = c
#define NOPAD  s = c

#define PRINT(var, size, format, args...)      \
	        fwd = snprintf(var, size, format, ##args); \
		c += (fwd >= size) ? size : fwd;

#define PRINT_PROGRESS(cur, total)			\
		int i = PROGRESS_LEN * cur / total;	\
		int j = PROGRESS_LEN - i;		\
							\
		while (i-- > 0) {			\
			PRINT(c, TAIL, "X");		\
		}					\
		while (j-- > 0) {			\
			PRINT(c, TAIL, ".");		\
		}					\
		PRINT(c, TAIL, " %i/%i", cur, total)

int
snprint_map_header (char * line, int len, char * format,
	            struct map_layout * ml)
{
	char * c = line;   /* line cursor */
	char * s = line;   /* for padding */
	char * f = format; /* format string cursor */
	int fwd;

	do {
		if (!TAIL)
			break;

		if (*f != '%') {
			*c++ = *f;
			NOPAD;
			continue;
		}
		f++;
		switch (*f) {
		case 'w':	
			PRINT(c, TAIL, "name");
			ml->mapname_len = MAX(ml->mapname_len, 4);
			PAD(ml->mapname_len);
			break;
		case 'd':
			PRINT(c, TAIL, "sysfs");
			ml->mapdev_len = MAX(ml->mapdev_len, 5);
			PAD(ml->mapdev_len);
			break;
		case 'F':
			PRINT(c, TAIL, "failback");
			ml->failback_progress_len =
				MAX(ml->failback_progress_len, 8);
			PAD(ml->failback_progress_len);
			break;
		case 'Q':
			PRINT(c, TAIL, "queueing");
			ml->queueing_progress_len =
				MAX(ml->queueing_progress_len, 8);
			PAD(ml->queueing_progress_len);
			break;
		case 'n':
			PRINT(c, TAIL, "paths");
			ml->nr_active_len = MAX(ml->nr_active_len, 5);
			PAD(ml->nr_active_len);
			break;
		case 't':
			PRINT(c, TAIL, "dm-st");
			PAD(7);
			break;
		default:
			break;
		}
	} while (*f++);

	line[c - line - 1] = '\n';
	line[c - line] = '\0';

	return (c - line);
}

int
snprint_map (char * line, int len, char * format,
	     struct multipath * mpp, struct map_layout * ml)
{
	char * c = line;   /* line cursor */
	char * s = line;   /* for padding */
	char * f = format; /* format string cursor */
	int fwd;

	do {
		if (!TAIL)
			break;

		if (*f != '%') {
			*c++ = *f;
			NOPAD;
			continue;
		}
		f++;
		switch (*f) {
		case 'w':	
			if (mpp->alias) {
				PRINT(c, TAIL, "%s", mpp->alias);
			} else {
				PRINT(c, TAIL, "%s", mpp->wwid);
			}
			PAD(ml->mapname_len);
			break;
		case 'd':
			PRINT(c, TAIL, "dm-%i", mpp->minor);
			PAD(ml->mapdev_len);
			break;
		case 'F':
			if (!mpp->failback_tick) {
				PRINT(c, TAIL, "-");
			} else {
				PRINT_PROGRESS(mpp->failback_tick,
					       mpp->pgfailback);
			}
			PAD(ml->failback_progress_len);
			break;
		case 'Q':
			if (mpp->no_path_retry == NO_PATH_RETRY_FAIL) {
				PRINT(c, TAIL, "off");
			} else if (mpp->no_path_retry == NO_PATH_RETRY_QUEUE) {
				PRINT(c, TAIL, "on");
			} else if (mpp->no_path_retry == NO_PATH_RETRY_UNDEF) {
				PRINT(c, TAIL, "-");
			} else if (mpp->no_path_retry > 0) {
				if (mpp->retry_tick) {
					PRINT(c, TAIL, "%i sec", mpp->retry_tick);
				} else {
					PRINT(c, TAIL, "%i chk", mpp->no_path_retry);
				}
			}
			PAD(ml->queueing_progress_len);
			break;
		case 'n':
			PRINT(c, TAIL, "%i", mpp->nr_active);
			PAD(ml->nr_active_len);
			break;
		case 't':
			if (mpp->dmstate == MAPSTATE_ACTIVE) {
				PRINT(c, TAIL, "active");
			} else if (mpp->dmstate == MAPSTATE_SUSPENDED) {
				PRINT(c, TAIL, "suspend");
			} else {
				PRINT(c, TAIL, "-");
			}
			PAD(7);
			break;
		default:
			break;
		}
	} while (*f++);

	line[c - line - 1] = '\n';
	line[c - line] = '\0';

	return (c - line);
}

int
snprint_path_header (char * line, int len, char * format,
		     struct path_layout * pl)
{
	char * c = line;   /* line cursor */
	char * s = line;   /* for padding */
	char * f = format; /* format string cursor */
	int fwd;

	do {
		if (!TAIL)
			break;

		if (*f != '%') {
			*c++ = *f;
			NOPAD;
			continue;
		}
		f++;
		switch (*f) {
		case 'w':
			PRINT(c, TAIL, "uuid");
			PAD(pl->uuid_len);
			break;
		case 'i':
			PRINT(c, TAIL, "hcil");
			PAD(pl->hbtl_len);
			break;
		case 'd':
			PRINT(c, TAIL, "dev");
			pl->dev_len = MAX(pl->dev_len, 3);
			PAD(pl->dev_len);
			break;
		case 'D':
			PRINT(c, TAIL, "dev_t");
			pl->dev_t_len = MAX(pl->dev_t_len, 5);
			PAD(pl->dev_t_len);
			break;
		case 'T':
			PRINT(c, TAIL, "chk-st");
			PAD(8);
			break;
		case 't':
			PRINT(c, TAIL, "dm-st");
			PAD(8);
			break;
		case 's':
			PRINT(c, TAIL, "vendor/product/rev");
			NOPAD;
			break;
		case 'C':
			PRINT(c, TAIL, "next-check");
			NOPAD;
			break;
		case 'p':
			PRINT(c, TAIL, "pri");
			pl->prio_len = MAX(pl->prio_len, 3);
			PAD(pl->prio_len);
			break;
		default:
			break;
		}
	} while (*f++);

	line[c - line - 1] = '\n';
	line[c - line] = '\0';

	return (c - line);
}

int
snprint_path (char * line, int len, char * format, struct path * pp,
	    struct path_layout * pl)
{
	char * c = line;   /* line cursor */
	char * s = line;   /* for padding */
	char * f = format; /* format string cursor */
	int fwd;

	do {
		if (!TAIL)
			break;

		if (*f != '%') {
			*c++ = *f;
			NOPAD;
			continue;
		}
		f++;
		switch (*f) {
		case 'w':	
			PRINT(c, TAIL, "%s", pp->wwid);
			PAD(pl->uuid_len);
			break;
		case 'i':
			if (pp->sg_id.host_no < 0) {
				PRINT(c, TAIL, "#:#:#:#");
			} else {
				PRINT(c, TAIL, "%i:%i:%i:%i",
					pp->sg_id.host_no,
					pp->sg_id.channel,
					pp->sg_id.scsi_id,
					pp->sg_id.lun);
			}
			PAD(pl->hbtl_len);
			break;
		case 'd':
			if (!strlen(pp->dev)) {
				PRINT(c, TAIL, "-");
			} else {
				PRINT(c, TAIL, "%s", pp->dev);
			}
			PAD(pl->dev_len);
			break;
		case 'D':
			PRINT(c, TAIL, "%s", pp->dev_t);
			PAD(pl->dev_t_len);
			break;
		case 'T':
			switch (pp->state) {
			case PATH_UP:
				PRINT(c, TAIL, "[ready]");
				break;
			case PATH_DOWN:
				PRINT(c, TAIL, "[faulty]");
				break;
			case PATH_SHAKY:
				PRINT(c, TAIL, "[shaky]");
				break;
			case PATH_GHOST:
				PRINT(c, TAIL, "[ghost]");
				break;
			default:
				PRINT(c, TAIL, "[undef]");
				break;
			}
			PAD(8);
			break;
		case 't':
			switch (pp->dmstate) {
			case PSTATE_ACTIVE:
				PRINT(c, TAIL, "[active]");
				break;
			case PSTATE_FAILED:
				PRINT(c, TAIL, "[failed]");
				break;
			default:
				PRINT(c, TAIL, "[undef]");
				break;
			}
			PAD(8);
			break;
		case 's':
			PRINT(c, TAIL, "%s/%s/%s",
				      pp->vendor_id, pp->product_id, pp->rev);
			NOPAD;
			break;
		case 'C':
			if (!pp->mpp) {
				PRINT(c, TAIL, "[orphan]");
			} else {
				PRINT_PROGRESS(pp->tick, pp->checkint);
			}
			NOPAD;
			break;
		case 'p':
			if (pp->priority) {
				PRINT(c, TAIL, "%i", pp->priority);
			} else {
				PRINT(c, TAIL, "#");
			}
			PAD(pl->prio_len);
			break;
		default:
			break;
		}
	} while (*f++);

	line[c - line - 1] = '\n';
	line[c - line] = '\0';

	return (c - line);
}

