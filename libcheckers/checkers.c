#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "checkers.h"

extern int
devnode (int action, char *devt)
{
	dev_t dev;
	int major, minor;
	int ret = 0;
	char devnode[DEVNODE_SIZE];

	if (snprintf(devnode, sizeof(devnode), "/tmp/.checkpath.%s",
		     devt) >= sizeof(devnode)) {
		fprintf(stderr, "checkpath: devnode too small\n");
		return -1;
	}

	switch (action)
	{
	case CREATE_NODE:
		sscanf(devt, "%i:%i", &major, &minor);
		dev = makedev(major, minor);
		unlink(devnode);
		ret = mknod(devnode, S_IFBLK | S_IRUSR | S_IWUSR, dev);
		break;

	case UNLINK_NODE:
		unlink(devnode);
		break;

	case OPEN_NODE:
		ret = open(devnode, O_RDONLY);
		break;

	default:
		break;
	}
	return(ret);
}
