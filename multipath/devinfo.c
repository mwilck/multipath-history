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

	return 0;
}

/*
 * Supported Code Set values.
 */
#define        CODESET_BINARY  1
#define        CODESET_ASCII   2

static const char hex_str[]="0123456789abcdef";

/*
 * check_fill_0x83_id - check the page 0x83 id, if OK allocate and fill
 * serial number.
 */
static int check_0x83_id(char *scsi_dev, char *page_83, int max_len)
{
	int i, j, len, idweight;

	/*
	 * ASSOCIATION must be with the device (value 0)
	 */
	if ((page_83[1] & 0x30) != 0)
		return -1;

	/*
	 * Check for code set - ASCII or BINARY.
	 */
	dbg("%s: Codeset: %X", scsi_dev, page_83[0] & 0x0f);
	if (((page_83[0] & 0x0f) != CODESET_ASCII) &&
	    ((page_83[0] & 0x0f) != CODESET_BINARY))
		return -1;

	/*
	 * Check for ID type
	 */
	idweight = page_83[1] & 0x0f;
	
#if DEBUG
	fprintf(stdout,"%s: ID type: ", scsi_dev);
	switch (page_83[1] & 0x0f) {
	case 0x0:
		printf("Vendor specific\n");
		break;
	case 0x1:
		printf("T-10 vendor identification\n");
		break;
	case 0x2:
		printf("EUI-64\n");
		break;
	case 0x3:
		printf("NAA ");
		switch((page_83[4] >> 4) & 0x0f) {
		case 0x02:
			printf("IEEE Extended\n");
			break;
		case 0x05:
			printf("IEEE Registered\n");
			break;
		case 0x06:
			printf("IEEE Registered Extended\n");
			break;
		default:
			printf("Reserved (%X)\n", (page_83[4] >> 4) & 0x0f);
			break;
		}
		break;
	case 0x4:
		printf("Relative target port\n");
		break;
	case 0x5:
		printf("Target port group\n");
		break;
	case 0x6:
		printf("Logical unit group\n");
		break;
	case 0x7:
		printf("MD5 logical unit identifier\n");
		break;
	default:
		printf("Reserved\n");
		break;
	}
#endif
	/*
	 * Only allow vendor specific, T-10 Vendor Identifcation,
	 * EUI-64 or NAA identificators
	 */
	if ((page_83[1] & 0x0f) > 0x3)
		return -1;

	/*
	 * Prefer NAA Registered Extended to NAA Registered and
	 * NAA Extended.
	 * Not checking for actual NAA types as some devices
	 * ( e.g. Compaq iSCSI disks via CISCO routers) return
	 * add NAA subtypes (0x3).
	 */
	if ((page_83[1] & 0x0f) == 0x03) {
		idweight += (page_83[4] >> 4) & 0x0f;

	}

	/*
	 * page_83[3]: identifier length
	 */
	len = page_83[3];
	if ((page_83[0] & 0x0f) != CODESET_ASCII)
		/*
		 * If not ASCII, use two bytes for each binary value.
		 */
		len *= 2;
	/*
	 * Add one byte for the NULL termination.
	 */
	if (max_len < len + 1) {
		fprintf(stderr, "%s: length %d too short - need %d\n",
			scsi_dev, max_len, len);
		return 1;
	}

	i = 4; /* offset to the start of the identifier */
	j = 0;

	if ((page_83[0] & 0x0f) == CODESET_ASCII) {
		/*
		 * ASCII descriptor.
		 *
		 * Check whether the descriptor contains anything useful
		 * i.e. anything apart from ' ' or '0'.
		 */
#if DEBUG
		fprintf(stdout,"%s: string '", scsi_dev);
#endif
		while (i < (4 + page_83[3])) {
			if (page_83[i] == ' ' || page_83[i] == '0')
				len--;
#if DEBUG
			fputc(page_83[i],stdout);
#endif
			i++;
		}
#if DEBUG
		fprintf(stdout,"'");
#endif
	} else {
		/*
		 * Binary descriptor, convert to ASCII, using two bytes of
		 * ASCII for each byte in the page_83.
		 *
		 * Again, check whether the descriptor contains anything
		 * useful; in this case, anything > 0.
		 */
#if DEBUG
		fprintf(stdout,"%s: binary ", scsi_dev);
#endif
		while (i < (4 + page_83[3])) {
			if (page_83[i] == 0)
				len-=2;
#if DEBUG
			fputc(hex_str[(page_83[i] & 0xf0) >> 4],stdout);
			fputc(hex_str[page_83[i] & 0x0f],stdout);
#endif
			i++;
		}
	}
	dbg(" (len %d)", len);

	if (len <= 0)
		return -1;

	return idweight;
}

/*
 * fill_0x83_id - fill serial number.
 */
static int fill_0x83_id(char *page_83, char *serial)
{
	int i, j, len;

	/*
	 * page_83[3]: identifier length
	 */
	len = page_83[3];
	if ((page_83[0] & 0x0f) != CODESET_ASCII)
		/*
		 * If not ASCII, use two bytes for each binary value.
		 */
		len *= 2;

	/*
	 * Add one byte for the NULL termination.
	 */
	len += 1;

	i = 4; /* offset to the start of the identifier */
	j = 0;

	if ((page_83[0] & 0x0f) == CODESET_ASCII) {
		/*
		 * ASCII descriptor.
		 */
		while (i < (4 + page_83[3])) {
			/* Map ' ' -> '_' */
			if (page_83[i] == ' ')
				serial[j] = '_';
			else
				serial[j] = page_83[i];
			j++; i++;
		}
	} else {
		/*
		 * Binary descriptor, convert to ASCII, using two bytes of
		 * ASCII for each byte in the page_83.
		 */
		while (i < (4 + page_83[3])) {
			serial[j++] = hex_str[(page_83[i] & 0xf0) >> 4];
			serial[j++] = hex_str[page_83[i] & 0x0f];
			i++;
		}
	}
	return 0;
}

unsigned long
get_disk_size (char * devname) {
	unsigned long size;
	char attr_path[FILE_NAME_SIZE];
	char sysfs_path[FILE_NAME_SIZE];
	char buff[FILE_NAME_SIZE];

	if (0 == sysfs_get_mnt_path(sysfs_path, FILE_NAME_SIZE)) {
		memset(attr_path, 0, FILE_NAME_SIZE);
		if(safe_sprintf(attr_path, "%s/block/%s/size",
			sysfs_path, devname)) {
			fprintf(stderr, "attr_path too small\n");
			return -1;
		}
		if (0 > sysfs_read_attribute_value(attr_path, buff,
			FILE_NAME_SIZE))
			return -1;
		size = strtoul(buff, NULL, 10);
		return size;
	}
	dbg("get_disk_size need sysfs");
	return -1;
}

/*
 * get EVPD page 0x83 off 8
 * tested ok with StorageWorks
 */
int
get_evpd_wwid (char * dev_t, char * wwid)
{
        int fd, j, weight, weight_cur, offset_cur, retval = 0;
        char buff[MX_ALLOC_LEN + 1];

	fd = opennode(dev_t, O_RDONLY);

        if (fd < 0)
		return 1;

	weight_cur = -1;
	offset_cur = -1;

        if (0 == do_inq(fd, 0, 1, 0x83, buff, MX_ALLOC_LEN, 1)) {
		/*
		 * Examine each descriptor returned. There is normally only
		 * one or a small number of descriptors.
		 */
		for (j = 4; j <= buff[3] + 3; j += buff[j + 3] + 4) {
#if DEBUG
			fprintf(stdout,"%s: ID descriptor at %d:\n", 
				dev_t, j);
#endif
			weight = check_0x83_id(dev_t, &buff[j], 
					       MX_ALLOC_LEN);
			if (weight >= 0 && weight > weight_cur) {
				weight_cur = weight;
				offset_cur = j;
			}
		}

		if (weight_cur >= 0)
			fill_0x83_id(&buff[offset_cur], wwid);
		else
			retval = 1;
	} else
		retval = 1;

	closenode(dev_t, fd);
        return retval;
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

	if (!buff)
		goto fallback;

	if (execute_program(buff, pp->wwid, WWID_SIZE) == 0) {
		dbg("uid = %s (callout)", pp->wwid);
		return 0;
	}

	fallback:
	if (!get_evpd_wwid(pp->dev_t, pp->wwid)) {
		dbg("uid = %s (internal getuid)", pp->wwid);
		return 0;
	}
	/*
	 * no wwid : blank for safety
	 */
	dbg("uid = 0x0 (unable to fetch)");
	memset(pp->wwid, 0, WWID_SIZE);
	return 1;
}

