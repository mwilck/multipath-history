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

#define TUR_CMD_LEN 6
#define HEAVY_CHECK_COUNT       10

#define MSG_TUR_UP	"tur checker report path is up"
#define MSG_TUR_DOWN	"tur checker report path is down"

struct tur_checker_context {
	int run_count;
	char wwn[64];
};

int tur(char *devnode, char *msg, void *context)
{
        unsigned char turCmdBlk[TUR_CMD_LEN] = { 0x00, 0, 0, 0, 0, 0 };
        struct sg_io_hdr io_hdr;
        unsigned char sense_buffer[32];
	int fd;
	struct tur_checker_context * ctxt;

	if (context != NULL) {
		ctxt = (struct tur_checker_context *)context;
		ctxt->run_count += 1;

		if (ctxt->run_count % HEAVY_CHECK_COUNT) {
			ctxt->run_count = 0;
			/* do stuff */
		}
	}

	fd = open (devnode, O_RDONLY);

        memset(&io_hdr, 0, sizeof (struct sg_io_hdr));
        io_hdr.interface_id = 'S';
        io_hdr.cmd_len = sizeof (turCmdBlk);
        io_hdr.mx_sb_len = sizeof (sense_buffer);
        io_hdr.dxfer_direction = SG_DXFER_NONE;
        io_hdr.cmdp = turCmdBlk;
        io_hdr.sbp = sense_buffer;
        io_hdr.timeout = 20000;
        io_hdr.pack_id = 0;
        if (ioctl(fd, SG_IO, &io_hdr) < 0) {
                close (fd);
		MSG(MSG_TUR_DOWN);
                return PATH_DOWN;
        }
        if (io_hdr.info & SG_INFO_OK_MASK) {
		close (fd);
		MSG(MSG_TUR_DOWN);
                return PATH_DOWN;
        }
        close (fd);
	if (msg != NULL)
		snprintf(msg, MAX_CHECKER_MSG_SIZE, "%s\n", MSG_TUR_UP);
	
	MSG(MSG_TUR_UP);
        return PATH_UP;
}
