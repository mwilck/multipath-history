#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>

#include "vector.h"
#include "structs.h"
#include "cache.h"

int
cache_load (vector pathvec)
{
	int fd;
	struct flock fl;
	off_t record_len;
	struct path record;
	struct path * pp;

	fd = open(CACHE_FILE, O_RDONLY);

	if (fd < 0)
		return 1;

	fl.l_type = F_RDLCK;
	fl.l_whence = 0;
	fl.l_start = 0;
	fl.l_len = 0;

	alarm(MAX_WAIT);

	if (fcntl(fd, F_SETLKW, &fl) == -1) {
		fprintf(stderr, "can't take a read lease on %s\n", CACHE_FILE);
		close(fd);
		return 1;
	}
	alarm(0);

	record_len = sizeof(struct path);

	while (read(fd, &record, record_len)) {
		pp = alloc_path();
		vector_alloc_slot(pathvec);
		vector_set_slot(pathvec, pp);
		memcpy(pp, &record, record_len);
		pp->fd = 0;
	}
	close(fd);
	return 0;
}

int
cache_dump (vector pathvec)
{
	int i;
	int fd;
	struct flock fl;
	off_t record_len;
	struct path * pp;

	fd = open(CACHE_FILE, O_RDWR|O_CREAT);

	if (fd < 0)
		return 1;

	fl.l_type = F_WRLCK;
	fl.l_whence = 0;
	fl.l_start = 0;
	fl.l_len = 0;

	alarm(MAX_WAIT);

	if (fcntl(fd, F_SETLKW, &fl) == -1) {
		fprintf(stderr, "can't take a write lease on %s\n", CACHE_FILE);
		close(fd);
		return 1;
	}
	alarm(0);
	ftruncate(fd, 0); 
	record_len = sizeof(struct path);

	vector_foreach_slot (pathvec, pp, i) {
		if (write(fd, pp, record_len) < record_len) {
			close(fd);
			return 1;
		}
	}
	close(fd);
	return 0;
}

int
cache_flush (void)
{
	int fd;
	struct flock fl;

	fd = open(CACHE_FILE, O_RDWR|O_CREAT);

	if (fd < 0)
		return 1;

	fl.l_type = F_WRLCK;
	fl.l_whence = 0;
	fl.l_start = 0;
	fl.l_len = 0;

	alarm(MAX_WAIT);

	if (fcntl(fd, F_SETLKW, &fl) == -1) {
		fprintf(stderr, "can't take a write lease on %s\n", CACHE_FILE);
		close(fd);
		return 1;
	}
	alarm(0);

	return ftruncate(fd, 0);
}

int
cache_cold (int expire)
{
	time_t t;
	struct stat s;

	if (time(&t) < 0)
		return 1;

	if(stat(CACHE_FILE, &s))
		return 1;

	if ((t - s.st_mtime) < expire)
		return 0;

	return 1;
}
