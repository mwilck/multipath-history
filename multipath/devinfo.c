#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sysfs/libsysfs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include "devinfo.h"
#include "sg_include.h"
#include "debug.h"
#include "config.h"

void
basename(char * str1, char * str2)
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
	uint32_t major;
	uint32_t minor;
	int fd;

	sscanf(devt, "%u:%u", &major, &minor);

	/* first, try with udev reverse mappings */
	sprintf(devpath, "%s/reverse/%u:%u", conf->udev_dir, major, minor);
	fd = open(devpath, mode);

	if (fd >= 0)
		return fd;

	/* fallback to temp devnode creation */
	memset(devpath, 0, FILE_NAME_SIZE);
	sprintf(devpath, "/tmp/.multipath.%u.%u.devnode", major, minor);
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
	uint32_t major;
	uint32_t minor;

	if (fd >= 0)		/* as it should always be */
		close(fd);

	sscanf(devt, "%u:%u", &major, &minor);
	sprintf(devpath, "/tmp/.multipath.%u.%u.devnode", major, minor);
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

int
sysfs_devinfo(struct path * curpath)
{
	char attr_path[FILE_NAME_SIZE];
	char attr_buff[32];
	char sysfs_path[FILE_NAME_SIZE];
	struct stat buf;

	if (0 == sysfs_get_mnt_path(sysfs_path, FILE_NAME_SIZE)) {
		sprintf(attr_path, "%s/block/%s", sysfs_path, curpath->dev);
		
		if(stat(attr_path, &buf))
			return 1;

		/* sysfs style */
		sprintf(attr_path, "%s/block/%s/device/vendor",
			sysfs_path, curpath->dev);
		if (0 > sysfs_read_attribute_value(attr_path,
			attr_buff, sizeof(attr_buff))) return 1;
		memcpy(curpath->vendor_id, attr_buff, 8);
 
		sprintf(attr_path, "%s/block/%s/device/model",
			sysfs_path, curpath->dev);
		if (0 > sysfs_read_attribute_value(attr_path,
			attr_buff, sizeof(attr_buff))) return 1;
		memcpy(curpath->product_id, attr_buff, 16);
 
		sprintf(attr_path, "%s/block/%s/device/rev",
			sysfs_path, curpath->dev);
		if (0 > sysfs_read_attribute_value(attr_path,
			attr_buff, sizeof(attr_buff))) return 1;
		memcpy(curpath->rev, attr_buff, 4);
 
		sprintf(attr_path, "%s/block/%s/dev",
			sysfs_path, curpath->dev);
		if (0 > sysfs_read_attribute_value(attr_path,
			attr_buff, sizeof(attr_buff))) return 1;
		sprintf(curpath->dev_t, "%s", attr_buff);
	} else {
		printf("need sysfs mounted : out\n");
		exit(1);
	}
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
		sprintf(attr_path, "%s/block/%s/size",
			sysfs_path, devname);
		if (0 > sysfs_read_attribute_value(attr_path, buff,
			FILE_NAME_SIZE))
			return -1;
		size = strtoul(buff, NULL, 10);
		return size;
	}
	dbg("get_disk_size need sysfs");
	return -1;
}

int
do_tur(char *devt)
{
	unsigned char turCmdBlk[TUR_CMD_LEN] = { 0x00, 0, 0, 0, 0, 0 };
	struct sg_io_hdr io_hdr;
	unsigned char sense_buffer[32];
	int fd;

	fd = opennode(devt, O_RDONLY);
	
	if (fd < 0)
		return 0;

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
		closenode(devt, fd);
		return 0;
	}

	closenode(devt, fd);
	
	if (io_hdr.info & SG_INFO_OK_MASK)
		return 0;

	return 1;
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
