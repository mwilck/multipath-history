#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>

#define FILESIZE 128
#define LOG(x, y, z...) if (DEBUG>=x) syslog(x, "[copy.c] " y, ##z)

static void
basename(char * str1, char * str2)
{
	char *p = str1 + (strlen(str1) - 1);

	while (*--p != '/')
		continue;
	strcpy(str2, ++p);
}

int
copy (char * src, char * dst)
{
	int fdin;
	int fdout;
	char * mmsrc;
	char * mmdst;
	struct stat statbuf;

	fdin = open (src, O_RDONLY);

	if (fdin < 0) {
		LOG(1, "cannot open %s", src);
		return -1;
	}
	/*
	 * Stat the input file to obtain its size
	 */
	if (fstat (fdin, &statbuf) < 0) {
		LOG(1, "cannot stat %s", src);
		goto out1;
	}
	/*
	 * Open the output file for writing,
	 * with the same permissions as the source file
	 */
	fdout = open (dst, O_RDWR | O_CREAT | O_TRUNC, statbuf.st_mode);

	if (fdout < 0) {
		LOG(1, "cannot open %s", dst);
		goto out1;
	}

	if (lseek (fdout, statbuf.st_size - 1, SEEK_SET) == -1) {
		LOG(1, "cannot lseek %s", dst);
		goto out2;
	}

	if (write (fdout, "", 1) != 1) {
		LOG(1, "cannot write dummy char");
		goto out2;
	}
	/*
	 * Blast the bytes from one file to the other
	 */
	if ((mmsrc = mmap(0, statbuf.st_size, PROT_READ, MAP_SHARED, fdin, 0))
		== (caddr_t) -1) {
		LOG(1, "cannot mmap %s", src);
		goto out2;
	}
	
	if ((mmdst = mmap(0, statbuf.st_size, PROT_READ | PROT_WRITE,
		MAP_SHARED, fdout, 0)) == (caddr_t) -1) {
		LOG(1, "cannot mmap %s", dst);
		goto out3;
	}
	memcpy(mmdst, mmsrc, statbuf.st_size);

/* done */
	munmap(mmdst, statbuf.st_size);
out3:
	munmap(mmsrc, statbuf.st_size);
out2:
	close (fdout);
out1:
	close (fdin);

	return 0;
}

int
copytodir (char * src, char * dstdir)
{
	char dst[FILESIZE];
	char filename[FILESIZE];
	
	basename(src, filename);
	if (FILESIZE <= snprintf(dst, FILESIZE, "%s/%s", dstdir, filename)) {
		LOG(1, "filename buffer overflow : %s ", filename);
		return -1;
	}

	return copy(src, dst);
}
