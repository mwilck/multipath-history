#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define MEGS 140 /* default; override on command line */
#define MEG (1024 * 1024)

int main (int argc, char *argv[])
{
	void **data;
	int i, r;
	size_t megs = MEGS;

	if ((argc >= 2) && (atoi(argv[1]) > 0))
		megs = atoi(argv[1]);

	data = malloc (megs * sizeof (void*));
	if (!data) abort();

	memset (data, 0, megs * sizeof (void*));

	srand(time(NULL));

	for (i = 0; i < megs; i++) {
		data[i] = malloc(MEG);
		memset (data[i], i, MEG);
		printf("malloc/memset %03d/%03lu\n", i+1, megs);
	}
	for (i = megs - 1; i >= 0; i--) {
		r = rand() % 200;
		memset (data[i], r, MEG);
		printf("memset #2 %03d/%03lu = %d\n", i+1, megs, r);
	}
	printf("done\n");
	sleep(99999);
	return 0;
}
