#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "sg_include.h"
#include "path_state.h"
#include "checkers.h"

#define INQUIRY_CMD     0x12
#define INQUIRY_CMDLEN  6
#define HEAVY_CHECK_COUNT	10

#define MSG_EMC_CLARIION_UP	"emc_clariion checker report path up"
#define MSG_EMC_CLARIION_DOWN	"emc_clariion checker report path down"
#define MSG_EMC_CLARIION_SHAKY	"emc_clariion checker report path is " \
				"scheduled for shutdown"

struct emc_clariion_checker_context {
	int run_count;
	char wwn[64];
};

int emc_clariion(char *devnode, char *msg, void *context)
{
	unsigned char sense_buffer[128];
	unsigned char inqCmdBlk[INQUIRY_CMDLEN] = {INQUIRY_CMD, 0, 0xC0, 0,
						sizeof(sense_buffer), 0};
	struct sg_io_hdr io_hdr;
	int fd;
	struct emc_clariion_checker_context * ctxt;

	if (context != NULL) {
		ctxt = (struct emc_clariion_checker_context *)context;
		ctxt->run_count++;
		/* do stuff */
	}

	if (ctxt->run_count % HEAVY_CHECK_COUNT) {
		ctxt->run_count = 0;
		/* do stuff */
	}

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
		MSG(MSG_EMC_CLARIION_DOWN);
		return PATH_DOWN;
	}
	if (io_hdr.info & SG_INFO_OK_MASK) {
		close (fd);
		MSG(MSG_EMC_CLARIION_DOWN);
		return PATH_DOWN;
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
		MSG(MSG_EMC_CLARIION_DOWN);
		return PATH_DOWN;
	}

	/*
	 * TODO: If we had a path_checker context per path, I could
	 * store the LUN WWN there and compare that it indeed did not
	 * change in between, to protect against the path suddenly
	 * pointing somewhere else.
	 */
	MSG(MSG_EMC_CLARIION_UP);
        return PATH_UP;

	/*
	 * TODO: determine the condition for that
	 */
	if (0)
		MSG(MSG_EMC_CLARIION_SHAKY);
}
