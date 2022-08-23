/*
 * Copyright (c) 2004, 2005 Christophe Varoqui
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

#define SENSE_BUFF_LEN 32

#define MSG_READSECTOR0_UP	"readsector0 checker reports path is up"
#define MSG_READSECTOR0_DOWN	"readsector0 checker reports path is down"

struct readsector0_checker_context {
	void * dummy;
};

int readsector0_init (struct checker * c)
{
	return 0;
}

void readsector0_free (struct checker * c)
{
	return;
}

static int
sg_read (int sg_fd, unsigned char * buff)
{
	/* defaults */
	int blocks = 1;
	long long start_block = 0;
	int bs = 512;
	int cdbsz = 10;
	int * diop = NULL;

	unsigned char rdCmd[cdbsz];
	unsigned char senseBuff[SENSE_BUFF_LEN];
	struct sg_io_hdr io_hdr;
	int res;
	int rd_opcode[] = {0x8, 0x28, 0xa8, 0x88};
	int sz_ind;
	
	memset(rdCmd, 0, cdbsz);
	sz_ind = 1;
	rdCmd[0] = rd_opcode[sz_ind];
	rdCmd[2] = (unsigned char)((start_block >> 24) & 0xff);
	rdCmd[3] = (unsigned char)((start_block >> 16) & 0xff);
	rdCmd[4] = (unsigned char)((start_block >> 8) & 0xff);
	rdCmd[5] = (unsigned char)(start_block & 0xff);
	rdCmd[7] = (unsigned char)((blocks >> 8) & 0xff);
	rdCmd[8] = (unsigned char)(blocks & 0xff);

	memset(&io_hdr, 0, sizeof(struct sg_io_hdr));
	io_hdr.interface_id = 'S';
	io_hdr.cmd_len = cdbsz;
	io_hdr.cmdp = rdCmd;
	io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
	io_hdr.dxfer_len = bs * blocks;
	io_hdr.dxferp = buff;
	io_hdr.mx_sb_len = SENSE_BUFF_LEN;
	io_hdr.sbp = senseBuff;
	io_hdr.timeout = DEF_TIMEOUT;
	io_hdr.pack_id = (int)start_block;
	if (diop && *diop)
	io_hdr.flags |= SG_FLAG_DIRECT_IO;

	while (((res = ioctl(sg_fd, SG_IO, &io_hdr)) < 0) && (EINTR == errno));

	if (res < 0) {
		if (ENOMEM == errno) {
			return PATH_UP;
		}
		return PATH_DOWN;
	}

	if ((0 == io_hdr.status) &&
	    (0 == io_hdr.host_status) &&
	    (0 == io_hdr.driver_status)) {
		return PATH_UP;
	} else {
		return PATH_DOWN;
	}
}

extern int
readsector0 (struct checker * c)
{
	unsigned char buf[512];
	int ret;

	ret = sg_read(c->fd, &buf[0]);

	switch (ret)
	{
	case PATH_DOWN:
		MSG(c, MSG_READSECTOR0_DOWN);
		break;
	case PATH_UP:
		MSG(c, MSG_READSECTOR0_UP);
		break;
	default:
		break;
	}
	return ret;
}
