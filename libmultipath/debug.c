#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "config.h"

void condlog (int prio, char * fmt, ...)
{
	va_list ap;

	if (!conf)
		return;

	va_start(ap, fmt);

	if (prio <= conf->verbosity) {
		vfprintf(stdout, fmt, ap);
		fprintf(stdout, "\n");
	}
	va_end(ap);
}
