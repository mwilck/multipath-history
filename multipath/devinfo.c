#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <safe_printf.h>

#include <sysfs/dlist.h>
#include <sysfs/libsysfs.h>
#include <memory.h>
#include <callout.h>
#include <util.h>
#include <vector.h>
#include <structs.h>

#include "sysfs_devinfo.h"
#include "devinfo.h"
#include "sg_include.h"
#include "debug.h"
#include "config.h"
#include "propsel.h"
#include "hwtable.h"

#define readattr(a,b) \
	sysfs_read_attribute_value(a, b, sizeof(b))

static int
opennode (char * devt, int mode)
{
	char devpath[FILE_NAME_SIZE];
	unsigned int major;
	unsigned int minor;
	int fd;

	sscanf(devt, "%u:%u", &major, &minor);

	/* first, try with udev reverse mappings */
	if (safe_sprintf(devpath, "%s/reverse/%u:%u",
			 conf->udev_dir, major, minor)) {
		fprintf(stderr, "devpath too small\n");
		return -1;
	}
	fd = open(devpath, mode);

	if (fd >= 0)
		return fd;

	/* fallback to temp devnode creation */
	memset(devpath, 0, FILE_NAME_SIZE);
	
	if (safe_sprintf(devpath, "/tmp/.multipath.%u.%u.devnode",
			 major, minor)) {
		fprintf(stderr, "devpath too small\n");
		return -1;
	}
	unlink (devpath);
	mknod(devpath, S_IFBLK|S_IRUSR|S_IWUSR, makedev(major, minor));
	fd = open(devpath, mode);
	
	if (fd < 0)
		unlink(devpath);

	return fd;

}

static void
closenode (char * devt, int fd)
{
	char devpath[FILE_NAME_SIZE];
	unsigned int major;
	unsigned int minor;

	if (fd >= 0)		/* as it should always be */
		close(fd);

	sscanf(devt, "%u:%u", &major, &minor);
	if (safe_sprintf(devpath, "/tmp/.multipath.%u.%u.devnode",
			 major, minor)) {
		fprintf(stderr, "devpath too small\n");
		return;
	}
	unlink(devpath);
}

int
get_claimed(char *devt)
{
	int fd;

	/*
	 * FIXME : O_EXCL always fails ?
	 */
	fd = opennode(devt, O_RDONLY);

	if (fd < 0)
		return 1;

	closenode(devt, fd);

	return 0;
}	

extern int
devt2devname (char *devname, char *devt)
{
	struct sysfs_directory * sdir;
	struct sysfs_directory * devp;
	char block_path[FILE_NAME_SIZE];
	char attr_path[FILE_NAME_SIZE];
	char attr_value[16];
	int len;

	if(safe_sprintf(block_path, "%s/block", sysfs_path)) {
		fprintf(stderr, "block_path too small\n");
		exit(1);
	}
	sdir = sysfs_open_directory(block_path);
	sysfs_read_directory(sdir);

	dlist_for_each_data (sdir->subdirs, devp, struct sysfs_directory) {
		if(safe_sprintf(attr_path, "%s/%s/dev",
				block_path, devp->name)) {
			fprintf(stderr, "attr_path too small\n");
			exit(1);
		}
		sysfs_read_attribute_value(attr_path, attr_value,
					   sizeof(attr_value));

		len = strlen(attr_value);

		/* discard newline */
		if (len > 1) len--;

		if (strlen(devt) == len &&
		    strncmp(attr_value, devt, len) == 0) {
			if(safe_sprintf(attr_path, "%s/%s",
					block_path, devp->name)) {
				fprintf(stderr, "attr_path too small\n");
				exit(1);
			}
			sysfs_get_name_from_path(attr_path, devname,
						 FILE_NAME_SIZE);
			sysfs_close_directory(sdir);
			return 0;
		}
	}
	sysfs_close_directory(sdir);
	return 1;
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
                return -1;
 
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
        return -1;
}

int
get_serial (char * str, char * devt)
{
	int fd;
        int len;
        char buff[MX_ALLOC_LEN + 1];

	fd = opennode(devt, O_RDONLY);

	if (fd < 0)
                return 0;

	if (0 == do_inq(fd, 0, 1, 0x80, buff, MX_ALLOC_LEN, 0)) {
		len = buff[3];
		if (len > 0) {
			memcpy(str, buff + 4, len);
			buff[len] = '\0';
		}
		close(fd);
		return 1;
	}

	closenode(devt, fd);
        return 0;
}

extern int
sysfs_devinfo(struct path * curpath)
{
	char attr_path[FILE_NAME_SIZE];
	char attr_buff[FILE_NAME_SIZE];

	if (sysfs_get_vendor(sysfs_path, curpath->dev,
			     curpath->vendor_id, SCSI_VENDOR_SIZE))
		return 1;
	dbg("vendor = %s", curpath->vendor_id);

	if (sysfs_get_model(sysfs_path, curpath->dev,
			    curpath->product_id, SCSI_PRODUCT_SIZE))
		return 1;
	dbg("product = %s", curpath->product_id);

	if (sysfs_get_rev(sysfs_path, curpath->dev,
			  curpath->rev, SCSI_REV_SIZE))
		return 1;
	dbg("rev = %s", curpath->rev);

	if (sysfs_get_dev(sysfs_path, curpath->dev,
			  curpath->dev_t, BLK_DEV_SIZE))
		return 1;
	dbg("dev_t = %s", curpath->dev_t);

	curpath->size = sysfs_get_size(sysfs_path, curpath->dev);

	if (curpath->size == 0)
		return 1;
	dbg("size = %lu", curpath->size);

	/*
	 * host / bus / target / lun
	 */
	if(safe_sprintf(attr_path, "%s/block/%s/device",
			sysfs_path, curpath->dev)) {
		fprintf(stderr, "attr_path too small\n");
		return 1;
	}
	if (0 > sysfs_get_link(attr_path, attr_buff, sizeof(attr_buff)))
		return 1;
	basename(attr_buff, attr_path);
	sscanf(attr_path, "%i:%i:%i:%i",
			&curpath->sg_id.host_no,
			&curpath->sg_id.channel,
			&curpath->sg_id.scsi_id,
			&curpath->sg_id.lun);
	dbg("h:b:t:l = %i:%i:%i:%i",
			curpath->sg_id.host_no,
			curpath->sg_id.channel,
			curpath->sg_id.scsi_id,
			curpath->sg_id.lun);

	/*
	 * target node name
	 */
	if(safe_sprintf(attr_path,
			"%s/class/fc_transport/target%i:%i:%i/node_name",
			sysfs_path,
			curpath->sg_id.host_no,
			curpath->sg_id.channel,
			curpath->sg_id.scsi_id)) {
		fprintf(stderr, "attr_path too small\n");
		return 1;
	}
	if (0 <= readattr(attr_path, attr_buff) && strlen(attr_buff) > 0)
		strncpy(curpath->tgt_node_name, attr_buff,
			strlen(attr_buff) - 1);
	dbg("tgt_node_name = %s", curpath->tgt_node_name);

	return 0;
}

static char *
apply_format (char * string, int maxsize, struct path * pp)
{
	char * pos;
	char * dst;
	char * p;
	int len;
	int myfree;

	if (!string)
		return NULL;

	dst = zalloc(maxsize);

	if (!dst)
		return NULL;

	p = dst;
	pos = strchr(string, '%');
	myfree = maxsize;

	if (!pos) {
		strcpy(dst, string);
		return dst;
	}

	len = (int) (pos - string) + 1;
	myfree -= len;

	if (myfree < 2) {
		free(dst);
		return NULL;
	}

	snprintf(p, len, "%s", string);
	p += len - 1;
	pos++;

	switch (*pos) {
	case 'n':
		len = strlen(pp->dev) + 1;
		myfree -= len;

		if (myfree < 2) {
			free(dst);
			return NULL;
		}

		snprintf(p, len, "%s", pp->dev);
		p += len - 1;
		break;
	case 'd':
		len = strlen(pp->dev_t) + 1;
		myfree -= len;

		if (myfree < 2) {
			free(dst);
			return NULL;
		}

		snprintf(p, len, "%s", pp->dev_t);
		p += len - 1;
		break;
	default:
		break;
	}
	pos++;

	if (!*pos)
		return dst;

	len = strlen(pos) + 1;
	myfree -= len;

	if (myfree < 2) {
		free(dst);
		return NULL;
	}

	snprintf(p, len, "%s", pos);
	dbg("reformated callout = %s", dst);
	return dst;
}

extern int
devinfo (struct path *pp)
{
	char * buff;
	char prio[16];

	dbg("===== path %s =====", pp->dev);

	/*
	 * fetch info available in sysfs
	 */
	if (sysfs_devinfo(pp))
		return 1;

	/*
	 * then those not available through sysfs
	 */
	get_serial(pp->serial, pp->dev_t);
	dbg("serial = %s", pp->serial);
	pp->claimed = get_claimed(pp->dev_t);
	dbg("claimed = %i", pp->claimed);

	/* get and store hwe pointer */
	pp->hwe = find_hwe(conf->hwtable, pp->vendor_id, pp->product_id);

	/*
	 * get path state, no message collection, no context
	 */
	select_checkfn(pp);
	pp->state = pp->checkfn(pp->dev_t, NULL, NULL);
	dbg("state = %i", pp->state);
	
	/*
	 * get path prio
	 */
	select_getprio(pp);
	buff = apply_format(pp->getprio, CALLOUT_MAX_SIZE, pp);

	if (!buff)
		pp->priority = 1;
	else if (execute_program(buff, prio, 16)) {
		dbg("error calling out %s", buff);
		pp->priority = 1;
		free(buff);
	} else
		pp->priority = atoi(prio);

	dbg("prio = %u", pp->priority);

	/*
	 * get path uid
	 */
	if (strlen(pp->wwid) == 0) {
		select_getuid(pp);
		buff = apply_format(pp->getuid, CALLOUT_MAX_SIZE, pp);

		if (buff) {
			if (!execute_program(buff, pp->wwid, WWID_SIZE) == 0)
				memset(pp->wwid, 0, WWID_SIZE);

			dbg("uid = %s (callout)", pp->wwid);
			free(buff);
		}
	} else
		dbg("uid = %s (cache)", pp->wwid);

	return 0;
}

