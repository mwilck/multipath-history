#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "checkers.h"

static int
makenode (char *devnode, char *devt)
{
	dev_t dev;
	int major, minor;

	sscanf(devt, "%i:%i", &major, &minor);

	dev = makedev(major, minor);
	unlink (devnode);

	return mknod(devnode, S_IFCHR | S_IRUSR | S_IWUSR, dev);
}

extern int
checkpath (char * devt, void * checkfn)
{
	int (*checker) (char *);
	char devnode[DEVNODE_SIZE];
	int r;

	checker = checkfn;

	if (checker <= 0 )
		return -1;

	if (snprintf(devnode, sizeof(devnode), "/tmp/.checkpath.%s",
		     devt) >= sizeof(devnode)) {
		fprintf(stderr, "checkpath: devnode too small\n");
		return -1;
	}
	if (makenode(devnode, devt))
		return -1;

	r = checker(devnode);
	unlink(devnode);
	return r;
}
