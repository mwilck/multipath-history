#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "main.h"
#include "configfile.h"

#define CONFIG		"/etc/multipath.conf"
#define MAXLINELENGHT	128
#define MAXWORDLENGHT	32

static int iscomment (char * line)
{
	char *p = line;

	while (*p == ' ' && *p != EOF && *p != '\n')
		p++;
	
	if (*p == '#' || *p == '!' || *p == ';' ||
			 *p == EOF || *p == '\n')
		return 1;

	return 0;
}

static char * getword (char * word, char * line)
{
	char *p = line;
	char *q;

	memset (word, 0, MAXWORDLENGHT * sizeof (char));
	while (*p != '"' && *p != EOF && *p != '\n')
		p++;

	if (*p == EOF || *p == '\n')
		return (NULL);

	q = ++p;
	
	if (*q == EOF || *q == '\n')
		return (NULL);


	while (*q != '"' && *q != EOF && *q != '\n')
		q++;
	
	if (*q == EOF || *q == '\n')
		return (NULL);

	if (q -p > MAXWORDLENGHT)
		return (NULL);

	strncpy (word, p, q - p);

	return (++q);
}

extern int check_config (void)
{
	struct stat buf;

	if (stat (CONFIG, &buf) < 0)
		return 0;

	return 1;
}

extern void * read_config (void ** getuid)
{
	FILE *fp;
	char *line;
	char *word;
	char *linep;
	int i = 0;
	struct hwentry * hwe = NULL;
	struct hwentry * hw;

	line = malloc (MAXLINELENGHT * sizeof (char));
	word = malloc (MAXWORDLENGHT * sizeof (char));
	hw = malloc (MAXHWENTRIES * sizeof (struct hwentry));
	
	fp = fopen (CONFIG, "r");

	if (!fp) {
		fprintf (stderr, "cannot fopen " CONFIG "\n");
		return NULL;
	}

	

	while (fgets (line, MAXLINELENGHT, fp))
	{
		if (iscomment (line))
			continue;

		hwe = &hw[i];
		memset (hwe, 0, sizeof (struct hwentry));
		linep = line;

		linep = getword (word, linep);
		if (linep == NULL)
			return NULL;
		
		strncpy (hwe->vendor, word, 16);

		linep = getword (word, linep);
		if (linep == NULL)
			return NULL;
		
		strncpy (hwe->product, word, 16);

		linep = getword (word, linep);
		if (linep == NULL)
			return NULL;
		
		hwe->iopolicy = atoi (word);

		if (hwe->iopolicy > HIGHESTPOLICY)
			hwe->iopolicy = 0;

		linep = getword (word, linep);
		if (linep == NULL)
			return NULL;
		
		hwe->getuid = getuid[atoi (word)];


		if (++i > MAXHWENTRIES) {
			hwe->getuid = NULL;
			return NULL;
		}
	}
	
	/* terminate array */
	hwe = &hw[i];
	hwe->getuid = NULL;
	
	fclose (fp);
	free (line);
	free (word);
	
	return hw;
}
