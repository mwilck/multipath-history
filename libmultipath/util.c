#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "debug.h"
#include "memory.h"

#define PARAMS_SIZE 255

int
strcmp_chomp(char *str1, char *str2)
{
	int i;
	char s1[PARAMS_SIZE],s2[PARAMS_SIZE];
 
	if(!str1 || !str2)
		return 1;

	strncpy(s1, str1, PARAMS_SIZE);
	strncpy(s2, str2, PARAMS_SIZE);

	for (i=strlen(s1)-1; i >=0 && isspace(s1[i]); --i) ;
	s1[++i] = '\0';
	for (i=strlen(s2)-1; i >=0 && isspace(s2[i]); --i) ;
	s2[++i] = '\0';

	return(strcmp(s1,s2));
}

void
strchop(char *str)
{
	int i;

	for (i=strlen(str)-1; i >=0 && isspace(str[i]); --i) ;
	str[++i] = '\0';
}

void
basename (char * str1, char * str2)
{
	char *p = str1 + (strlen(str1) - 1);

	while (*--p != '/' && p != str1)
		continue;

	if (p != str1)
		p++;

	strcpy(str2, p);
}

int
filepresent (char * run) {
	struct stat buf;

	if(!stat(run, &buf))
		return 1;
	return 0;
}

int
get_word (char * sentence, char ** word)
{
	char * p;
	int len;
	int skip = 0;
	
	if (word)
		*word = NULL;

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

	*word = MALLOC(len + 1);

	if (!*word) {
		condlog(0, "get_word : oom\n");
		return 0;
	}
	strncpy(*word, sentence, len);
	condlog(4, "*word = %s, len = %i", *word, len);

	if (*p == '\0')
		return 0;

	return skip + len;
}

