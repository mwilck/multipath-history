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

#include "devinfo.h"
#include "sg_include.h"
#include "debug.h"
#include "config.h"
#include "propsel.h"

#define readattr(a,b) \
	sysfs_read_attribute_value(a, b, sizeof(b))

void
basename (char * str1, char * str2)
{
        char *p = str1 + (strlen(str1) - 1);
 
        while (*--p != '/')
                continue;
        strcpy(str2, ++p);
}

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
	char sysfs_path[FILE_NAME_SIZE];
	char block_path[FILE_NAME_SIZE];
	char attr_path[FILE_NAME_SIZE];
	char attr_value[16];

	if (sysfs_get_mnt_path(sysfs_path, FILE_NAME_SIZE)) {
		fprintf(stderr, "feature available with sysfs only\n");
		exit(1);
	}
		
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

		if (!strncmp(attr_value, devt, strlen(devt))) {
			if(safe_sprintf(attr_path, "%s/%s",
					block_path, devp->name)) {
				fprintf(stderr, "attr_path too small\n");
				exit(1);
			}
			sysfs_get_name_from_path(attr_path, devname,
						 FILE_NAME_SIZE);
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
	char sysfs_path[FILE_NAME_SIZE];
	struct stat buf;

	if (sysfs_get_mnt_path(sysfs_path, FILE_NAME_SIZE)) {
		fprintf(stderr, "need sysfs mounted : out\n");
		return 1;
	}
	if (safe_sprintf(attr_path, "%s/block/%s", sysfs_path, curpath->dev)) {
		fprintf(stderr, "attr_path too small\n");
		return 1;
	}
	if(stat(attr_path, &buf))
		return 1;

	/*
	 * vendor string
	 */
	if(safe_sprintf(attr_path, "%s/block/%s/device/vendor",
			sysfs_path, curpath->dev)) {
		fprintf(stderr, "attr_path too small\n");
		return 1;
	}
	if (0 > readattr(attr_path, attr_buff))
		return 1;
	memcpy(curpath->vendor_id, attr_buff, 8);
	dbg("vendor = %s", curpath->vendor_id);

	/*
	 * model string
	 */
	if(safe_sprintf(attr_path, "%s/block/%s/device/model",
		sysfs_path, curpath->dev)) {
		fprintf(stderr, "attr_path too small\n");
		return 1;
	}
	if (0 > readattr(attr_path, attr_buff))
			return 1;
	memcpy(curpath->product_id, attr_buff, 16);
	dbg("product = %s", curpath->product_id);

	/*
	 * revision string
	 */
	if(safe_sprintf(attr_path, "%s/block/%s/device/rev",
		sysfs_path, curpath->dev)) {
		fprintf(stderr, "attr_path too small\n");
		return 1;
	}
	if (0 > readattr(attr_path, attr_buff))
		return 1;
	memcpy(curpath->rev, attr_buff, 4);
	dbg("rev = %s", curpath->rev);

	/*
	 * bdev major:minor string
	 */
	if(safe_sprintf(attr_path, "%s/block/%s/dev",
		sysfs_path, curpath->dev)) {
		fprintf(stderr, "attr_path too small\n");
		return 1;
	}
	if (0 > readattr(attr_path, attr_buff))
		return 1;
	if (strlen(attr_buff) > 1)
		strncpy(curpath->dev_t, attr_buff,
			strlen(attr_buff) - 1);
	dbg("dev_t = %s", curpath->dev_t);

	/*
	 * size
	 */
	if(safe_sprintf(attr_path, "%s/block/%s/size",
		sysfs_path, curpath->dev)) {
		fprintf(stderr, "attr_path too small\n");
		return 1;
	}
	if (0 > readattr(attr_path, attr_buff))
		return 1;
	curpath->size = strtoul(attr_buff, NULL, 0);
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
	 */
		return 1;
	}
		return 1;

}

static char *
apply_format (char * string, int maxsize, struct path * pp)
{
	char * pos;
	char * dst;
	char * p;
	int len;
	int free;

	if (!string)
		return NULL;

	dst = zalloc(maxsize);

	if (!dst)
		return NULL;

	p = dst;
	pos = strchr(string, '%');
	free = maxsize;

	if (!pos)
		return string;

	len = (int) (pos - string) + 1;
	free -= len;

	if (free < 2)
		return NULL;

	snprintf(p, len, "%s", string);
	p += len - 1;
	pos++;

	switch (*pos) {
	case 'n':
		len = strlen(pp->dev) + 1;
		free -= len;

		if (free < 2)
			return NULL;

		snprintf(p, len, "%s", pp->dev);
		p += len - 1;
		break;
	case 'd':
		len = strlen(pp->dev_t) + 1;
		free -= len;

		if (free < 2)
			return NULL;

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
	free -= len;

	if (free < 2)
		return NULL;

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
	} else
		pp->priority = atoi(prio);

	dbg("prio = %u", pp->priority);

	/*
	 * get path uid
	 */
	select_getuid(pp);
	buff = apply_format(pp->getuid, CALLOUT_MAX_SIZE, pp);

	if (buff && execute_program(buff, pp->wwid, WWID_SIZE) == 0) {
		dbg("uid = %s (callout)", pp->wwid);
		return 0;
	}

	/*
	 * no wwid : blank for safety
	 */
	dbg("uid = 0x0 (unable to fetch)");
	memset(pp->wwid, 0, WWID_SIZE);
	return 1;
}

