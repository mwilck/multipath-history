#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "sg_include.h"

#define INQUIRY_CMD     0x12
#define INQUIRY_CMDLEN  6

int emc_clariion(char *devnode)
{
	unsigned char sense_buffer[128];
	unsigned char inqCmdBlk[INQUIRY_CMDLEN] = {INQUIRY_CMD, 0, 0xC0, 0,
						sizeof(sense_buffer), 0};
	struct sg_io_hdr io_hdr;
	int fd;

	fd = open (devnode, O_RDONLY);

	memset(&io_hdr, 0, sizeof (struct sg_io_hdr));
	io_hdr.interface_id = 'S';
	io_hdr.cmd_len = sizeof (inqCmdBlk);
	io_hdr.mx_sb_len = sizeof (sense_buffer);
	io_hdr.dxfer_direction = SG_DXFER_NONE;
	io_hdr.cmdp = inqCmdBlk;
	io_hdr.sbp = sense_buffer;
	io_hdr.timeout = 20000;
	io_hdr.pack_id = 0;
	if (ioctl(fd, SG_IO, &io_hdr) < 0) {
		close (fd);
		return 0;
	}
	if (io_hdr.info & SG_INFO_OK_MASK) {
		close (fd);
		return 0;
	}
	close (fd);

	/*
	 * TODO: Better logging of why a path is considered failed? But
	 * then, the other checkers don't do that either...
	 */
	if (        /* Verify the code page - right page & revision */
		sense_buffer[1] != 0xc0 || sense_buffer[9] != 0x00
		/* Effective initiator type */
		|| sense_buffer[27] != 0x03
		/* Failover mode should be set to 1 */        
		|| (sense_buffer[28] & 0x07) != 0x04
		/* Arraycommpath should be set to 1 */
		|| (sense_buffer[30] & 0x04) != 0x04
		/* LUN operations should indicate normal operations */
		|| sense_buffer[48] != 0x00
		/* LUN should at least be bound somewhere */
		|| sense_buffer[4] != 0x00) {
		return 0;
	}

	/*
	 * TODO: If we had a path_checker context per path, I could
	 * store the LUN WWN there and compare that it indeed did not
	 * change in between, to protect against the path suddenly
	 * pointing somewhere else.
	 */
        return 1;
}
