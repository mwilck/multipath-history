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

struct emc_clariion_checker_context {
	int run_count;
	char wwn[16];
	unsigned wwn_set;
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
		MSG("emc_clariion_checker: sending query command failed");
		return PATH_DOWN;
	}
	if (io_hdr.info & SG_INFO_OK_MASK) {
		close (fd);
		MSG("emc_clariion_checker: query command indicates error");
		return PATH_DOWN;
	}
	close (fd);

	if (/* Verify the code page - right page & revision */
	    sense_buffer[1] != 0xc0 || sense_buffer[9] != 0x00) {
		MSG("emc_clariion_checker: Path unit report page in unknown format");
		return PATH_DOWN;
	}

	if ( /* Effective initiator type */
	    	sense_buffer[27] != 0x03
		/* Failover mode should be set to 1 */        
		|| (sense_buffer[28] & 0x07) != 0x04
		/* Arraycommpath should be set to 1 */
		|| (sense_buffer[30] & 0x04) != 0x04) {
		MSG("emc_clariion_checker: Path not correctly configured for failover");
		return PATH_DOWN;
	}

	if ( /* LUN operations should indicate normal operations */
		sense_buffer[48] != 0x00) {
		MSG("emc_clariion_checker: Path not available for normal operations");
		return PATH_SHAKY;
	}

#if 0
	/* This is not actually an error as the failover to this group
	 * _would_ bind the path */
	if ( /* LUN should at least be bound somewhere */
		sense_buffer[4] != 0x00) {
		return PATH_UP;
	}
#endif	
	
	/*
	 * store the LUN WWN there and compare that it indeed did not
	 * change in between, to protect against the path suddenly
	 * pointing somewhere else.
	 */

	if (ctxt->wwn_set) {
		if (!memcmp(ctxt->wwn, &sense_buffer[10], 16)) {
			MSG("emc_clariion_checker: Logical Unit WWN has changed!");
			return PATH_DOWN;
		}
	} else {
		memcpy(ctxt->wwn, &sense_buffer[10], 16);
		ctxt->wwn_set = 1;
	}
	
	
	MSG("emc_clariion_checker: Path healthy");
        return PATH_UP;

}
