/*
 * Given a block device and a partition table type,
 * try to parse the partition table, and list the
 * contents. Optionally add or remove partitions.
 *
 * Read wholedisk and add all partitions:
 *	kpartx [-a|-d|-l] [-v] wholedisk
 *
 * aeb, 2000-03-21
 * cva, 2002-10-26
 */

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ctype.h>

#include "../libdevmapper/libdevmapper.h"
#include "kpartx.h"
#include "crc32.h"

/* loop devices */
#include "lopart.h"

#define SIZE(a) (sizeof(a)/sizeof((a)[0]))

#define MAXTYPES	64
#define MAXSLICES	256
#define DM_TARGET	"linear"
#define LO_NAME_SIZE    64

struct slice slices[MAXSLICES];

enum action { LIST, ADD, DELETE };

struct pt {
	char *type;
	ptreader *fn;
} pts[MAXTYPES];

int ptct;

static void
addpts(char *t, ptreader f)
{
	if (ptct >= MAXTYPES) {
		fprintf(stderr, "addpts: too many types\n");
		exit(1);
	}
	pts[ptct].type = t;
	pts[ptct].fn = f;
	ptct++;
}

static void
initpts(void)
{
	addpts("gpt", read_gpt_pt);
	addpts("dos", read_dos_pt);
	addpts("bsd", read_bsd_pt);
	addpts("solaris", read_solaris_pt);
	addpts("unixware", read_unixware_pt);
}

/* Used in gpt.c */
int force_gpt=0;

static int
usage(void) {
	printf("usage : kpartx [-a|-d|-l] [-v] wholedisk\n");
	printf("\t-a add partition devmappings\n");
	printf("\t-d del partition devmappings\n");
	printf("\t-l list partitions devmappings that would be added by -a\n");
	printf("\t-v verbose\n");
	return 1;
}

static int
dm_simplecmd (int task, const char *name) {
	int r = 0;
	struct dm_task *dmt;

	if (!(dmt = dm_task_create (task)))
		return 0;

	if (!dm_task_set_name (dmt, name))
		goto out;

	r = dm_task_run (dmt);

	out:
	dm_task_destroy (dmt);
	return r;
}

static int
dm_addmap (int task, const char *name, const char *params, long size) {
        struct dm_task *dmt;

        if (!(dmt = dm_task_create (task)))
                return 0;

        if (!dm_task_set_name (dmt, name))
                goto addout;

        if (!dm_task_add_target (dmt, 0, size, DM_TARGET, params))
                goto addout;

        if (!dm_task_run (dmt))
                goto addout;

        addout:
        dm_task_destroy (dmt);
        return 1;
}

static int
map_present (char * str)
{
	int r = 0;
	struct dm_task *dmt;
	struct dm_names *names;
	unsigned next = 0;

	if (!(dmt = dm_task_create (DM_DEVICE_LIST)))
		return 0;

        if (!dm_task_run (dmt))
		goto out;

	if (!(names = dm_task_get_names (dmt)))
		goto out;

	if (!names->dev)
		goto out;

	do {
		if (0 == strcmp (names->name, str))
			r = 1;

		next = names->next;
		names = (void *) names + next;
	} while (next);

	out:
	dm_task_destroy (dmt);
	return r;
}

static void
set_delimiter (char * device, char * delimiter)
{
	char * p = device;

	while (*(p++) != 0x0)
		continue;

	if (isdigit (*(p - 2)))
		*delimiter = 'p';
}

static void
strip_slash (char * device)
{
	char * p = device;

	while (*(p++) != 0x0) {
		
		if (*p == '/')
			*p = '!';
	}
}

int
main(int argc, char **argv){
        int fd, i, j, k, n, op;
	struct slice all;
	struct pt *ptp;
	enum action what = LIST;
	char *p, *type, *diskdevice, *device;
	int lower, upper;
	int verbose = 0;
	struct dm_task *dmt;
	char partname[20], params[30];
	char * loopdev = NULL;
	char delim[8] = "";
	int loopro = 0;
	struct stat buf;

	initpts();
	init_crc32();

	lower = upper = 0;
	type = device = diskdevice = NULL;
	
	if (argc < 2) {
		usage();
		exit(1);
	}

	for (i = 1; i < argc; ++i) {
		if (0 == strcmp("-g", argv[i])) {
			force_gpt=1;
			continue;
		}
		if (0 == strcmp("-t", argv[i])) {
			strcpy(type, argv[++i]);
			continue;
		}
		if (0 == strcmp("-v", argv[i])) {
			verbose = 1;
			continue;
		}
		if (0 == strcmp("-n", argv[i])) {
			p = argv[++i];
			lower = atoi(p);
			if ((p[1] == '-') && p[2])
				upper = atoi(p+2);
			else
				upper = lower;
			continue;
		}

		if (0 == strcmp("-l", argv[i])) {
			what = LIST;
			continue;
		}
		else if (0 == strcmp("-a", argv[i])) {
			what = ADD;
			continue;
		}
		else if (0 == strcmp("-d", argv[i])) {
			what = DELETE;
			continue;
		}
		
		if ((i == argc-2) && (argv[i][0] != '-')) {
			device = argv[i];
			diskdevice = argv[++i];
			break;
		}

		if ((i == argc-1) && (argv[i][0] != '-')) {
			diskdevice = device = argv[i];
			break;
		}

		usage ();
		exit (1);
	}

	if (stat (device, &buf)) {
		printf("failed to stat() device\n");
		exit (1);
	}

	if (S_ISREG (buf.st_mode)) {
		loopdev = malloc(LO_NAME_SIZE * sizeof (char));
		
		if (!loopdev)
			exit (1);

		/* already looped file ? */
		loopdev = find_loop_by_file(device);

		if (!loopdev && what == DELETE)
			exit (0);
				
		if (!loopdev) {
			loopdev = find_unused_loop_device();

			if (set_loop (loopdev, device, 0, &loopro)) {
				fprintf (stderr,
					 "can't set up loop\n");
				exit (1);
			}
		}

		device = loopdev;
	}

	set_delimiter (device, &delim[0]);
			
	fd = open (device, O_RDONLY);

	if (fd == -1) {
		perror (device);
		exit (1);
	}

	if (!lower)
		lower = 1;

	/* add/remove partitions to the kernel devmapper tables */

	for (i = 0; i < ptct; i++) {

		ptp = &pts[i];

		if (type && strcmp (type, ptp->type) > 0)
			continue;
		
		/* here we get partitions */
		n = ptp->fn (fd, all, slices, SIZE(slices));

		if (n >= 0 && verbose)
			printf ("%s: %d slices\n", ptp->type, n);

		if (n > 0)
			close (fd);

		if (n > 0 && what == LIST) {

			for (j = 0; j < n; j++) {

				if (slices[j].size == 0)
					continue;

				printf ("%s%d : 0 %d %s %d\n",
				        device+5, j+1, slices[j].size, device,
				        slices[j].start);
			}
		}

		if (n > 0 && what == DELETE) {

			for (j = 0; j < n; j++) {

				sprintf (partname, 
					 "%s%s%d", device+5 , delim, j+1);

				strip_slash (partname);
			
				if (slices[j].size == 0)
					continue;

				if (!map_present (partname))
					continue;

				if (!(dmt = dm_task_create (DM_DEVICE_REMOVE)))
					return 0;

				if (!dm_task_set_name (dmt, partname))
					goto delout;
			
				if (!dm_task_run (dmt))
					goto delout;

				if (verbose)
					printf ("Deleted device map : %s\n",
						 partname);

				delout:
					dm_task_destroy (dmt);
			}

			if (S_ISREG (buf.st_mode)) {

				if (del_loop (device) && verbose) {
				    printf ("can't delete loop : %s\n", device);
				    exit (1);
				}

				printf ("loop deleted : %s\n", device);
			}
		}

		if (n > 0 && what == ADD) {

			/* test for overlap, as in the case of an
			   extended partition, and reduce size */

			for (j=0; j<n; j++) {

				for (k=j+1; k<n; k++) {

					if (slices[k].start > slices[j].start &&
					    slices[k].start < slices[j].start +
					    slices[j].size) {
						slices[j].size = slices[k].start -
								 slices[j].start;

						if (verbose)
							printf("reduced size of "
							       "partition #%d to %d\n",
							       lower+j, slices[j].size);
					}
				}
			}

			for (j=0; j<n; j++) {

				if (slices[j].size == 0)
					continue;

				sprintf(partname, "%s%s%d", 
					device+5 , delim, j+1);

				strip_slash (partname);

				sprintf(params, "%s %d", device, (unsigned) slices[j].start);

				op = (map_present (partname) ? DM_DEVICE_RELOAD : DM_DEVICE_CREATE);

				dm_addmap (op, partname, params, (unsigned) slices[j].size);

				if (op == DM_DEVICE_RELOAD)
					dm_simplecmd (DM_DEVICE_RESUME, partname);

				if (verbose)
					printf("Added %s : 0 %d %s %s\n",
						partname, slices[j].size,
						DM_TARGET, params);
			}
		}

		if (n > 0)
			break;
	}
	return 0;
}

void *
xmalloc (size_t size) {
	void *t;

	if (size == 0)
		return NULL;

	t = malloc (size);

	if (t == NULL) {
		fprintf(stderr, "Out of memory\n");
		exit(1);
	}

	return t;
}

/*
 * sseek: seek to specified sector
 */
#if !defined (__alpha__) && !defined (__ia64__)
#include <linux/unistd.h>       /* _syscall */
static
_syscall5(int,  _llseek,  uint,  fd, ulong, hi, ulong, lo,
	  long long *, res, uint, wh);
#endif

static int
sseek(int fd, unsigned int secnr) {
	long long in, out;
	in = ((long long) secnr << 9);
	out = 1;

#if !defined (__alpha__) && !defined (__ia64__)
	if (_llseek (fd, in>>32, in & 0xffffffff, &out, SEEK_SET) != 0
	    || out != in)
#else
	if ((out = lseek(fd, in, SEEK_SET)) != in)
#endif
	{
		fprintf(stderr, "llseek error\n");
		return -1;
	}
	return 0;
}

static
struct block {
	unsigned int secnr;
	char *block;
	struct block *next;
} *blockhead;

char *
getblock (int fd, unsigned int secnr) {
	struct block *bp;

	for (bp = blockhead; bp; bp = bp->next)

		if (bp->secnr == secnr)
			return bp->block;

	if (sseek(fd, secnr))
		return NULL;

	bp = xmalloc(sizeof(struct block));
	bp->secnr = secnr;
	bp->next = blockhead;
	blockhead = bp;
	bp->block = (char *) xmalloc(1024);
	
	if (read(fd, bp->block, 1024) != 1024) {
		fprintf(stderr, "read error, sector %d\n", secnr);
		bp->block = NULL;
	}

	return bp->block;
}
