/* 
 * Part:        Configuration file parser/reader. Place into the dynamic
 *              data structure representation the conf file
 *  
 * Version:     $Id: parser.c,v 1.0.3 2003/05/11 02:28:03 acassen Exp $
 * 
 * Author:      Alexandre Cassen, <acassen@linux-vs.org>
 *              
 *              This program is distributed in the hope that it will be useful,
 *              but WITHOUT ANY WARRANTY; without even the implied warranty of
 *              MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *              See the GNU General Public License for more details.
 *
 *              This program is free software; you can redistribute it and/or
 *              modify it under the terms of the GNU General Public License
 *              as published by the Free Software Foundation; either version
 *              2 of the License, or (at your option) any later version.
 */

#include <syslog.h>

#include "parser.h"
#include "memory.h"

/* local vars */
static int sublevel = 0;

void
keyword_alloc(vector keywords, char *string, void (*handler) (vector))
{
	struct keyword *keyword;

	vector_alloc_slot(keywords);

	keyword = (struct keyword *) zalloc(sizeof (struct keyword));
	keyword->string = string;
	keyword->handler = handler;

	vector_set_slot(keywords, keyword);
}

void
install_keyword_root(char *string, void (*handler) (vector))
{
	keyword_alloc(keywords, string, handler);
}

void
install_sublevel(void)
{
	sublevel++;
}

void
install_sublevel_end(void)
{
	sublevel--;
}

void
install_keyword(char *string, void (*handler) (vector))
{
	int i = 0;
	struct keyword *keyword;

	/* fetch last keyword */
	keyword = VECTOR_SLOT(keywords, VECTOR_SIZE(keywords) - 1);

	/* position to last sub level */
	for (i = 0; i < sublevel; i++)
		keyword =
		    VECTOR_SLOT(keyword->sub, VECTOR_SIZE(keyword->sub) - 1);

	/* First sub level allocation */
	if (!keyword->sub)
		keyword->sub = vector_alloc();

	/* add new sub keyword */
	keyword_alloc(keyword->sub, string, handler);
}

void
free_keywords(vector keywords)
{
	struct keyword *keyword;
	int i;

	for (i = 0; i < VECTOR_SIZE(keywords); i++) {
		keyword = VECTOR_SLOT(keywords, i);
		if (keyword->sub)
			free_keywords(keyword->sub);
		free(keyword);
	}
	vector_free(keywords);
}

vector
alloc_strvec(char *string)
{
	char *cp, *start, *token;
	int strlen;
	int in_string;
	vector strvec;

	if (!string)
		return NULL;

	cp = string;

	/* Skip white spaces */
	while (isspace((int) *cp) && *cp != '\0')
		cp++;

	/* Return if there is only white spaces */
	if (*cp == '\0')
		return NULL;

	/* Return if string begin with a comment */
	if (*cp == '!' || *cp == '#')
		return NULL;

	/* Create a vector and alloc each command piece */
	strvec = vector_alloc();

	in_string = 0;
	while (1) {
		start = cp;
		if (*cp == '"') {
			cp++;
			token = zalloc(2);
			*(token) = '"';
			*(token + 1) = '\0';
			if (in_string)
				in_string = 0;
			else
				in_string = 1;

		} else {
			while ((in_string || !isspace((int) *cp)) && *cp
				!= '\0' && *cp != '"')
				cp++;
			strlen = cp - start;
			token = zalloc(strlen + 1);
			memcpy(token, start, strlen);
			*(token + strlen) = '\0';
		}

		/* Alloc & set the slot */
		vector_alloc_slot(strvec);
		vector_set_slot(strvec, token);

		while (isspace((int) *cp) && *cp != '\0')
			cp++;
		if (*cp == '\0' || *cp == '!' || *cp == '#')
			return strvec;
	}
}

int
read_line(char *buf, int size)
{
	int ch;
	int count = 0;

	while ((ch = fgetc(stream)) != EOF && (int) ch != '\n'
	       && (int) ch != '\r') {
		if (count < size)
			buf[count] = (int) ch;
		else
			break;
		count++;
	}
	return (ch == EOF) ? 0 : 1;
}

vector
read_value_block(void)
{
	char *buf;
	int i;
	char *str = NULL;
	char *dup;
	vector vec = NULL;
	vector elements = vector_alloc();

	buf = (char *) zalloc(MAXBUF);
	while (read_line(buf, MAXBUF)) {
		vec = alloc_strvec(buf);
		if (vec) {
			str = VECTOR_SLOT(vec, 0);
			if (!strcmp(str, EOB)) {
				free_strvec(vec);
				break;
			}

			if (VECTOR_SIZE(vec))
				for (i = 0; i < VECTOR_SIZE(vec); i++) {
					str = VECTOR_SLOT(vec, i);
					dup = (char *) zalloc(strlen(str) + 1);
					memcpy(dup, str, strlen(str));
					vector_alloc_slot(elements);
					vector_set_slot(elements, dup);
				}
			free_strvec(vec);
		}
		memset(buf, 0, MAXBUF);
	}

	free(buf);
	return elements;
}

void
alloc_value_block(vector strvec, void (*alloc_func) (vector))
{
	char *buf;
	char *str = NULL;
	vector vec = NULL;

	buf = (char *) zalloc(MAXBUF);
	while (read_line(buf, MAXBUF)) {
		vec = alloc_strvec(buf);
		if (vec) {
			str = VECTOR_SLOT(vec, 0);
			if (!strcmp(str, EOB)) {
				free_strvec(vec);
				break;
			}

			if (VECTOR_SIZE(vec))
				(*alloc_func) (vec);

			free_strvec(vec);
		}
		memset(buf, 0, MAXBUF);
	}
	free(buf);
}

void *
set_value(vector strvec)
{
	char *str = VECTOR_SLOT(strvec, 1);
	int size = strlen(str);
	int i = 0;
	int len = 0;
	char *alloc = NULL;
	char *tmp;

	if (*str == '"') {
		for (i = 2; i < VECTOR_SIZE(strvec); i++) {
			str = VECTOR_SLOT(strvec, i);
			len += strlen(str);
			if (!alloc)
				alloc =
				    (char *) zalloc(sizeof (char *) *
						    (len + 1));
			else {
				alloc =
				    realloc(alloc, sizeof (char *) * (len + 1));
				tmp = VECTOR_SLOT(strvec, i-1);
				if (*str != '"' && *tmp != '"')
					strncat(alloc, " ", 1);
			}

			if (i != VECTOR_SIZE(strvec)-1)
				strncat(alloc, str, strlen(str));
		}
	} else {
		alloc = zalloc(sizeof (char *) * (size + 1));
		memcpy(alloc, str, size);
	}
	return alloc;
}

/* recursive configuration stream handler */
static int kw_level = 0;
void
process_stream(vector keywords)
{
	int i;
	struct keyword *keyword;
	char *str;
	char *buf;
	vector strvec;

	buf = zalloc(MAXBUF);
	if (!read_line(buf, MAXBUF)) {
		free(buf);
		return;
	}

	strvec = alloc_strvec(buf);
	free(buf);

	if (!strvec) {
		process_stream(keywords);
		return;
	}

	str = VECTOR_SLOT(strvec, 0);

	if (!strcmp(str, EOB) && kw_level > 0) {
		free_strvec(strvec);
		return;
	}

	for (i = 0; i < VECTOR_SIZE(keywords); i++) {
		keyword = VECTOR_SLOT(keywords, i);

		if (!strcmp(keyword->string, str)) {
			if (keyword->handler)
				(*keyword->handler) (strvec);

			if (keyword->sub) {
				kw_level++;
				process_stream(keyword->sub);
				kw_level--;
			}
			break;
		}
	}

	free_strvec(strvec);
	process_stream(keywords);
}

/* Data initialization */
void
init_data(char *conf_file, vector (*init_keywords) (void))
{
	stream = fopen(conf_file, "r");
	if (!stream) {
		syslog(LOG_WARNING, "Configuration file open problem");
		return;
	}

	/* Init Keywords structure */
	(*init_keywords) ();

/* Dump configuration *
  vector_dump(keywords);
  dump_keywords(keywords, 0);
*/

	/* Stream handling */
	process_stream(keywords);
	fclose(stream);
	free_keywords(keywords);
}
