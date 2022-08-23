/*
 * Copyright (c) 2005 Christophe Varoqui
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "checkers.h"

#include "../libmultipath/sg_include.h"

#define TUR_CMD_LEN		6
#define INQUIRY_CMDLEN		6
#define INQUIRY_CMD		0x12
#define SENSE_BUFF_LEN		32
#define SCSI_CHECK_CONDITION	0x2
#define SCSI_COMMAND_TERMINATED	0x22
#define SG_ERR_DRIVER_SENSE	0x08
#define RECOVERED_ERROR		0x01
#define MX_ALLOC_LEN		255
#define HEAVY_CHECK_COUNT       10

#define MSG_HP_SW_UP	"hp_sw checker reports path is up"
#define MSG_HP_SW_DOWN	"hp_sw checker reports path is down"
#define MSG_HP_SW_GHOST	"hp_sw checker reports path is ghost"

struct sw_checker_context {
	void * dummy;
};

int libcheck_init (struct checker * c)
{
	return 0;
}

void libcheck_free (struct checker * c)
{
	return;
}

static int
do_inq(int sg_fd, int cmddt, int evpd, unsigned int pg_op,
       void *resp, int mx_resp_len, int noisy)
{
	unsigned char inqCmdBlk[INQUIRY_CMDLEN] =
		{ INQUIRY_CMD, 0, 0, 0, 0, 0 };
	unsigned char sense_b[SENSE_BUFF_LEN];
	struct sg_io_hdr io_hdr;

	if (cmddt)
		inqCmdBlk[1] |= 2;
	if (evpd)
		inqCmdBlk[1] |= 1;
	inqCmdBlk[2] = (unsigned char) pg_op;
	inqCmdBlk[3] = (unsigned char)((mx_resp_len >> 8) & 0xff);
	inqCmdBlk[4] = (unsigned char) (mx_resp_len & 0xff);
	memset(&io_hdr, 0, sizeof (struct sg_io_hdr));
	memset(sense_b, 0, SENSE_BUFF_LEN);
	io_hdr.interface_id = 'S';
	io_hdr.cmd_len = sizeof (inqCmdBlk);
	io_hdr.mx_sb_len = sizeof (sense_b);
	io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
	io_hdr.dxfer_len = mx_resp_len;
	io_hdr.dxferp = resp;
	io_hdr.cmdp = inqCmdBlk;
	io_hdr.sbp = sense_b;
	io_hdr.timeout = DEF_TIMEOUT;

	if (ioctl(sg_fd, SG_IO, &io_hdr) < 0)
		return 1;

	/* treat SG_ERR here to get rid of sg_err.[ch] */
	io_hdr.status &= 0x7e;
	if ((0 == io_hdr.status) && (0 == io_hdr.host_status) &&
	    (0 == io_hdr.driver_status))
		return 0;
	if ((SCSI_CHECK_CONDITION == io_hdr.status) ||
	    (SCSI_COMMAND_TERMINATED == io_hdr.status) ||
	    (SG_ERR_DRIVER_SENSE == (0xf & io_hdr.driver_status))) {
		if (io_hdr.sbp && (io_hdr.sb_len_wr > 2)) {
			int sense_key;
			unsigned char * sense_buffer = io_hdr.sbp;
			if (sense_buffer[0] & 0x2)
				sense_key = sense_buffer[1] & 0xf;
			else
				sense_key = sense_buffer[2] & 0xf;
			if(RECOVERED_ERROR == sense_key)
				return 0;
		}
	}
	return 1;
}

static int
do_tur (int fd)
{
	unsigned char turCmdBlk[TUR_CMD_LEN] = { 0x00, 0, 0, 0, 0, 0 };
	struct sg_io_hdr io_hdr;
	unsigned char sense_buffer[32];

	memset(&io_hdr, 0, sizeof (struct sg_io_hdr));
	io_hdr.interface_id = 'S';
	io_hdr.cmd_len = sizeof (turCmdBlk);
	io_hdr.mx_sb_len = sizeof (sense_buffer);
	io_hdr.dxfer_direction = SG_DXFER_NONE;
	io_hdr.cmdp = turCmdBlk;
	io_hdr.sbp = sense_buffer;
	io_hdr.timeout = DEF_TIMEOUT;
	io_hdr.pack_id = 0;

	if (ioctl(fd, SG_IO, &io_hdr) < 0)
		return 1;

	if (io_hdr.info & SG_INFO_OK_MASK)
		return 1;

	return 0;
}

extern int
libcheck_check (struct checker * c)
{
	char buff[MX_ALLOC_LEN];

	if (0 != do_inq(c->fd, 0, 1, 0x80, buff, MX_ALLOC_LEN, 0)) {
		MSG(c, MSG_HP_SW_DOWN);
		return PATH_DOWN;
	}

	if (do_tur(c->fd)) {
		MSG(c, MSG_HP_SW_GHOST);
		return PATH_GHOST;
	}
	MSG(c, MSG_HP_SW_UP);
	return PATH_UP;
}
