/*
 * Christophe Varoqui (2004)
 * This code is GPLv2, see license file
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vector.h"
#include "memory.h"
#include "structs.h"
#include "util.h"

#define WORD_SIZE 64

#if DEBUG
#define dbg(format, arg...) fprintf(stderr, format "\n", ##arg)
#else
#define dbg(format, arg...) do {} while(0)
#endif

static int
get_word (char * sentence, char ** word)
{
	char * p;
	int len;
	int skip = 0;
	
	while (*sentence ==  ' ') {
		sentence++;
		skip++;
	}
	if (*sentence == '\0')
		return 0;

	p = sentence;

	while (*p !=  ' ' && *p != '\0')
		p++;

	len = (int) (p - sentence);

	if (!word)
		return skip + len;

	*word = zalloc(len + 1);

	if (!*word) {
		fprintf(stderr, "get_word : oom\n");
		return 0;
	}
	strncpy(*word, sentence, len);
//	dbg("*word = %s, len = %i", *word, len);

	if (*p == '\0')
		return 0;

	return skip + len;
}

static int
merge_words (char ** dst, char * word, int space)
{
	char * p;
	int len;

	len = strlen(*dst) + strlen(word) + space;
	*dst = realloc(*dst, len + 1);

	if (!*dst)
		return 1;

	p = *dst;

	while (*p != '\0')
		p++;

	while (space) {
		*p = ' ';
		p++;
		space--;
	}
	strncpy(p, word, strlen(word) + 1);

	return 0;
}

extern void
disassemble_map (vector pathvec, char * params, struct multipath * mpp)
{
	char * word;
	char * p;
	int i, j, k;
	int num_features;
	int num_hwhandler;
	int num_pg;
	int num_pg_args;
	int num_paths;
	int num_paths_args;
	struct path * pp;
	struct pathgroup * pgp;

	p = params;

	/*
	 * features
	 */
	p += get_word(p, &mpp->features);
	num_features = atoi(mpp->features);

	for (i = 0; i < num_features; i++) {
		p += get_word(p, &word);
		merge_words(&mpp->features, word, 1);
		free(word);
	}

	/*
	 * hwhandler
	 */
	p += get_word(p, &mpp->hwhandler);
	num_hwhandler = atoi(mpp->hwhandler);

	for (i = 0; i < num_hwhandler; i++) {
		p += get_word(p, &word);
		merge_words(&mpp->hwhandler, word, 1);
		free(word);
	}

	/*
	 * nb of path groups
	 */
	p += get_word(p, &word);
	num_pg = atoi(word);
	free(word);

	if (num_pg > 0 && !mpp->pg)
		mpp->pg = vector_alloc();
	
	/*
	 * first pg to try
	 */
	p += get_word(p, &word);
	mpp->nextpg = atoi(word);
	free(word);

	for (i = 0; i < num_pg; i++) {
		/*
		 * selector
		 */

		if (!mpp->selector) {
			p += get_word(p, &mpp->selector);
			/*
			 * selector args
			 */
			p += get_word(p, &word);
			num_pg_args = atoi(word);
			merge_words(&mpp->selector, word, 1);
			free(word);
		} else {
			p += get_word(p, NULL);
			p += get_word(p, NULL);
		}

		for (j = 0; j < num_pg_args; j++)
			p += get_word(p, NULL);

		/*
		 * paths
		 */
		pgp = zalloc(sizeof(struct pathgroup));
		pgp->paths = vector_alloc();
		vector_alloc_slot(mpp->pg);
		vector_set_slot(mpp->pg, pgp);

		p += get_word(p, &word);
		num_paths = atoi(word);
		free(word);

		p += get_word(p, &word);
		num_paths_args = atoi(word);
		free(word);

		for (j = 0; j < num_paths; j++) {
			pp = NULL;
			p += get_word(p, &word);
			if (pathvec)
				pp = find_path_by_devt(pathvec, word);

			if (!pp) {
				pp = alloc_path();
				strncpy(pp->dev_t, word, BLK_DEV_SIZE);
			}
			free(word);

			if (pp) {
				vector_alloc_slot(pgp->paths);
				vector_set_slot(pgp->paths, pp);
				pgp->id ^= (long)pp;
			}
			if (pp && !strlen(mpp->wwid))
				strncpy(mpp->wwid, pp->wwid, WWID_SIZE);

			for (k = 0; k < num_paths_args; k++)
				p += get_word(p, NULL);
		}
	}
}

extern void
disassemble_status (char * params, struct multipath * mpp)
{
	char * word;
	char * p;
	int i, j;
	int num_feature_args;
	int num_hwhandler_args;
	int num_pg;
	int num_pg_args;
	int num_paths;
	struct path * pp;
	struct pathgroup * pgp;

	p = params;

	/*
	 * features
	 */
	p += get_word(p, &word);
	num_feature_args = atoi(word);
	free(word);

	for (i = 0; i < num_feature_args; i++) {
		if (i == 1) {
			p += get_word(p, &word);
			mpp->queuedio = atoi(word);
			free(word);
			continue;
		}
		/* unknown */
		p += get_word(p, NULL);
	}
	/*
	 * hwhandler
	 */
	p += get_word(p, &word);
	num_hwhandler_args = atoi(word);
	free(word);

	for (i = 0; i < num_hwhandler_args; i++)
		p += get_word(p, NULL);

	/*
	 * nb of path groups
	 */
	p += get_word(p, &word);
	num_pg = atoi(word);
	free(word);

	/*
	 * next pg to try
	 */
	p += get_word(p, NULL);

	if (VECTOR_SIZE(mpp->pg) < num_pg)
		return;

	for (i = 0; i < num_pg; i++) {
		pgp = VECTOR_SLOT(mpp->pg, i);
		/*
		 * PG status
		 */
		p += get_word(p, &word);
		switch (*word) {
		case 'D':
			pgp->status = PGSTATE_DISABLED;
			break;
		case 'A':
			pgp->status = PGSTATE_ACTIVE;
			break;
		case 'E':
			pgp->status = PGSTATE_ENABLED;
			break;
		default:
			pgp->status = PGSTATE_RESERVED;
			break;
		}
		free(word);

		/*
		 * undef ?
		 */
		p += get_word(p, NULL);

		p += get_word(p, &word);
		num_paths = atoi(word);
		free(word);

		p += get_word(p, &word);
		num_pg_args = atoi(word);
		free(word);

		if (VECTOR_SIZE(pgp->paths) < num_paths)
			return;

		for (j = 0; j < num_paths; j++) {
			pp = VECTOR_SLOT(pgp->paths, j);
			/*
			 * path
			 */
			p += get_word(p, NULL);

			/*
			 * path status
			 */
			p += get_word(p, &word);
			switch (*word) {
			case 'F':
				pp->dmstate = PSTATE_FAILED;
				break;
			case 'A':
				pp->dmstate = PSTATE_ACTIVE;
				break;
			default:
				break;
			}
			free(word);
			/*
			 * fail count
			 */
			p += get_word(p, &word);
			pp->failcount = atoi(word);
			free(word);
		}
		/*
		 * selector args
		 */
		for (j = 0; j < num_pg_args; j++)
			p += get_word(p, NULL);
	}
}
