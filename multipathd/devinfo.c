#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sysfs/libsysfs.h>

#include <safe_printf.h>
#include "devinfo.h"

#define FILE_NAME_SIZE 255

void
basename(char * str1, char * str2)
{
	char *p = str1 + (strlen(str1) - 1);

	while (*--p != '/') 
		continue;
	strcpy(str2, ++p);
}

int
get_lun_strings(char * vendor_id, char * product_id, char * rev, char * devname)
{
	char attr_path[FILE_NAME_SIZE];
	char attr_buff[17];
	char sysfs_path[FILE_NAME_SIZE];

	if (sysfs_get_mnt_path (sysfs_path, FILE_NAME_SIZE))
		return 1;
		
	if(safe_sprintf(attr_path, "%s/block/%s/device/vendor",
		sysfs_path, devname)) {
		fprintf(stderr, "get_lun_strings: attr_path too small\n");
		return 1;
	}
	if (0 > sysfs_read_attribute_value(attr_path,
		attr_buff, 17)) return 0;
	memcpy (vendor_id, attr_buff, 8);

	if(safe_sprintf(attr_path, "%s/block/%s/device/model",
		sysfs_path, devname)) {
		fprintf(stderr, "get_lun_strings: attr_path too small\n");
		return 1;
	}
	if (0 > sysfs_read_attribute_value(attr_path,
		attr_buff, 17)) return 0;
	memcpy (product_id, attr_buff, 16);
 
	if(safe_sprintf(attr_path, "%s/block/%s/device/rev",
		sysfs_path, devname)) {
		fprintf(stderr, "get_lun_strings: attr_path too small\n");
		return 1;
	}
	if (0 > sysfs_read_attribute_value(attr_path,
		attr_buff, 17)) return 0;
	memcpy (rev, attr_buff, 4);

	return 0;
}
