#include <stdio.h>
#include <stdlib.h>
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

#define MSG_TUR_UP	"tur checker reports path is up"
#define MSG_TUR_DOWN	"tur checker reports path is down"

struct tur_checker_context {
	int fd;
	int run_count;
};


extern int
tur (char *devt, char *msg, void **context)
{
        unsigned char turCmdBlk[TUR_CMD_LEN] = { 0x00, 0, 0, 0, 0, 0 };
        struct sg_io_hdr io_hdr;
        unsigned char sense_buffer[32];
	struct tur_checker_context * ctxt = NULL;
	int ret;

	/*
	 * caller passed in a context : use its address
	 */
	if (context)
		ctxt = (struct tur_checker_context *) (*context);

	/*
	 * passed in context is uninitialized or volatile context :
	 * initialize it
	 */
	if (!ctxt) {
		ctxt = malloc(sizeof(struct tur_checker_context));
		memset(ctxt, 0, sizeof(struct tur_checker_context));

		if (!ctxt) {
			MSG("cannot allocate context");
			return -1;
		}
		if (context)
			*context = ctxt;
	}
	ctxt->run_count++;

	if (ctxt->run_count % HEAVY_CHECK_COUNT) {
		ctxt->run_count = 0;
		/* do stuff */
	}
	if (!ctxt->fd) {
		if (devnode(CREATE_NODE, devt)) {
			MSG("cannot create node");
			ret = -1;
			goto out;
		}
		ctxt->fd = devnode(OPEN_NODE, devt);
		devnode(UNLINK_NODE, devt);
	}
	
        memset(&io_hdr, 0, sizeof (struct sg_io_hdr));
        io_hdr.interface_id = 'S';
        io_hdr.cmd_len = sizeof (turCmdBlk);
        io_hdr.mx_sb_len = sizeof (sense_buffer);
        io_hdr.dxfer_direction = SG_DXFER_NONE;
        io_hdr.cmdp = turCmdBlk;
        io_hdr.sbp = sense_buffer;
        io_hdr.timeout = 20000;
        io_hdr.pack_id = 0;
        if (ioctl(ctxt->fd, SG_IO, &io_hdr) < 0) {
		MSG(MSG_TUR_DOWN);
                ret = PATH_DOWN;
		goto out;
        }
        if (io_hdr.info & SG_INFO_OK_MASK) {
		MSG(MSG_TUR_DOWN);
                ret = PATH_DOWN;
		goto out;
        }
	MSG(MSG_TUR_UP);
        ret = PATH_UP;

out:
	/*
	 * caller told us he doesn't want to keep the context :
	 * free it
	 */
	if (!context) {
		close(ctxt->fd);
		free(ctxt);
	}
	return(ret);
}
