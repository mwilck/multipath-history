#include <string.h>

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

