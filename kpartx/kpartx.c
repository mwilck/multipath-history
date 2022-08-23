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

#include "kpartx.h"
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ctype.h>
#include <libdevmapper.h>
#include <devmapper.h>

#include "crc32.h"

/* loop devices */
#include "lopart.h"

#define SIZE(a) (sizeof(a)/sizeof((a)[0]))

#define READ_SIZE	1024
#define MAXTYPES	64
#define MAXSLICES	256
#define DM_TARGET	"linear"
#define LO_NAME_SIZE    64
#define PARTNAME_SIZE	128
#define DELIM_SIZE	8

struct slice slices[MAXSLICES];

enum action { LIST, ADD, DELETE };

struct pt {
	char *type;
	ptreader *fn;
} pts[MAXTYPES];

int ptct = 0;

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

static char short_opts[] = "ladgvnp:t:";

/* Used in gpt.c */
int force_gpt=0;

static int
usage(void) {
	printf("usage : kpartx [-a|-d|-l] [-v] wholedisk\n");
	printf("\t-a add partition devmappings\n");
	printf("\t-d del partition devmappings\n");
	printf("\t-l list partitions devmappings that would be added by -a\n");
	printf("\t-p set device name-partition number delimiter\n");
	printf("\t-v verbose\n");
	return 1;
}

static void
set_delimiter (char * device, char * delimiter)
{
	char * p = device;

	while (*(p++) != 0x0)
		continue;

	if (isdigit(*(p - 2)))
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

static int
find_devname_offset (char * device)
{
	char *p, *q = NULL;
	
	p = device;
	
	while (*p++)
		if (*p == '/')
			q = p;

	return (int)(q - device) + 1;
}
			
int
main(int argc, char **argv){
        int fd, i, j, k, n, op, off, arg;
	struct slice all;
	struct pt *ptp;
	enum action what = LIST;
	char *p, *type, *diskdevice, *device;
	int lower, upper;
	int verbose = 0;
	char partname[PARTNAME_SIZE], params[PARTNAME_SIZE + 16];
	char * loopdev = NULL;
	char * delim = NULL;
	int loopro = 0;
	struct stat buf;

	initpts();
	init_crc32();

	lower = upper = 0;
	type = device = diskdevice = NULL;
	memset(&all, 0, sizeof(all));
	memset(&partname, 0, sizeof(partname));
	
	if (argc < 2) {
		usage();
		exit(1);
	}

	while ((arg = getopt(argc, argv, short_opts)) != EOF) switch(arg) {
		case 'g':
			force_gpt=1;
			break;
		case 't':
			type = optarg;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'n':
			p = optarg;
			lower = atoi(p);
			if ((p[1] == '-') && p[2])
				upper = atoi(p+2);
			else
				upper = lower;
			break;
		case 'p':
			delim = optarg;
			break;
		case 'l':
			what = LIST;
			break;
		case 'a':
			what = ADD;
			break;
		case 'd':
			what = DELETE;
			break;
		default:
			usage();
			exit(1);
	}

	if (dm_prereq(DM_TARGET, 0, 0, 0) && (what == ADD || what == DELETE)) {
		fprintf(stderr, "device mapper prerequisites not met\n"); 
		exit(1);
	}

	if (optind == argc-2) {
		device = argv[optind];
		diskdevice = argv[optind+1];
	} else if (optind == argc-1) {
		diskdevice = device = argv[optind];
	} else {
		usage();
		exit(1);
	}

	if (stat (device, &buf)) {
		printf("failed to stat() device\n");
		exit (1);
	}

	if (S_ISREG (buf.st_mode)) {
		loopdev = malloc(LO_NAME_SIZE * sizeof(char));
		
		if (!loopdev)
			exit(1);

		/* already looped file ? */
		loopdev = find_loop_by_file(device);

		if (!loopdev && what == DELETE)
			exit (0);
				
		if (!loopdev) {
			loopdev = find_unused_loop_device();

			if (set_loop(loopdev, device, 0, &loopro)) {
				fprintf(stderr, "can't set up loop\n");
				exit (1);
			}
		}
		device = loopdev;
	}

	if (delim == NULL) {
		delim = malloc(DELIM_SIZE);
		memset(delim, 0, DELIM_SIZE);
		set_delimiter(device, delim);
	}
	
	off = find_devname_offset(device);
	fd = open(device, O_RDONLY);

	if (fd == -1) {
		perror(device);
		exit(1);
	}
	if (!lower)
		lower = 1;

	/* add/remove partitions to the kernel devmapper tables */
	for (i = 0; i < ptct; i++) {
		ptp = &pts[i];

		if (type && strcmp(type, ptp->type) > 0)
			continue;
		
		/* here we get partitions */
		n = ptp->fn(fd, all, slices, SIZE(slices));

#ifdef DEBUG
		if (n >= 0)
			printf("%s: %d slices\n", ptp->type, n);
#endif

		if (n > 0)
			close(fd);
		else
			continue;

		/*
		 * test for overlap, as in the case of an extended partition
		 * zero their size to avoid mapping
		 */
		for (j=0; j<n; j++) {
			for (k=j+1; k<n; k++) {
				if (slices[k].start > slices[j].start &&
				    slices[k].start < slices[j].start +
				    slices[j].size)
					slices[j].size = 0;
			}
		}

		switch(what) {
		case LIST:
			for (j = 0; j < n; j++) {
				if (slices[j].size == 0)
					continue;

				printf("%s%s%d : 0 %lu %s %lu\n",
					device + off, delim, j+1,
					(unsigned long) slices[j].size, device,
				        (unsigned long) slices[j].start);
			}
			break;

		case DELETE:
			for (j = 0; j < n; j++) {
				if (safe_sprintf(partname, "%s%s%d",
					     device + off , delim, j+1)) {
					fprintf(stderr, "partname too small\n");
					exit(1);
				}
				strip_slash(partname);

				if (!slices[j].size || !dm_map_present(partname))
					continue;

				if (!dm_simplecmd(DM_DEVICE_REMOVE, partname))
					continue;

				if (verbose)
					printf("del devmap : %s\n", partname);
			}

			if (S_ISREG (buf.st_mode)) {
				if (del_loop(device)) {
					if (verbose)
				    		printf("can't del loop : %s\n",
							device);
					exit(1);
				}
				printf("loop deleted : %s\n", device);
			}
			break;

		case ADD:
			for (j=0; j<n; j++) {
				if (slices[j].size == 0)
					continue;

				if (safe_sprintf(partname, "%s%s%d",
					     device + off , delim, j+1)) {
					fprintf(stderr, "partname too small\n");
					exit(1);
				}
				strip_slash(partname);
				
				if (safe_sprintf(params, "%s %lu", device,
					     (unsigned long)slices[j].start)) {
					fprintf(stderr, "params too small\n");
					exit(1);
				}

				op = (dm_map_present(partname) ?
					DM_DEVICE_RELOAD : DM_DEVICE_CREATE);

				dm_addmap(op, partname, DM_TARGET, params,
					  slices[j].size);

				if (op == DM_DEVICE_RELOAD)
					dm_simplecmd(DM_DEVICE_RESUME,
							partname);

				if (verbose)
					printf("add map %s : 0 %lu %s %s\n",
						partname, slices[j].size,
						DM_TARGET, params);
			}
			break;

		default:
			break;

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
#if !defined (__alpha__) && !defined (__ia64__) && !defined (__x86_64__) \
	&& !defined (__s390x__)
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

#if !defined (__alpha__) && !defined (__ia64__) && !defined (__x86_64__) \
	&& !defined (__s390x__)
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
	bp->block = (char *) xmalloc(READ_SIZE);
	
	if (read(fd, bp->block, READ_SIZE) != READ_SIZE) {
		fprintf(stderr, "read error, sector %d\n", secnr);
		bp->block = NULL;
	}

	return bp->block;
}
