#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sysfs/libsysfs.h>

#include "safe_printf.h"
#include "sysfs_devinfo.h"

#define declare_sysfs_get_str(fname, fmt) \
extern int \
sysfs_get_##fname (char * sysfs_path, char * dev, char * buff, int len) \
{ \
	char attr_path[SYSFS_PATH_SIZE]; \
	char attr_buff[SYSFS_PATH_SIZE]; \
	int attr_len; \
\
	if(safe_sprintf(attr_path, fmt, sysfs_path, dev)) \
		return 1; \
	if (0 > sysfs_read_attribute_value(attr_path, attr_buff, sizeof(attr_buff))) \
		return 1; \
\
	attr_len = strlen(attr_buff); \
	if (attr_len < 2 || attr_len - 1 > len) \
		return 1; \
\
	strncpy(buff, attr_buff, attr_len - 1); \
	buff[attr_len - 1] = '\0'; \
	return 0; \
}

declare_sysfs_get_str(vendor, "%s/block/%s/device/vendor");
declare_sysfs_get_str(model, "%s/block/%s/device/model");
declare_sysfs_get_str(rev, "%s/block/%s/device/rev");
declare_sysfs_get_str(dev, "%s/block/%s/dev");

#define declare_sysfs_get_val(fname, fmt) \
extern unsigned long  \
sysfs_get_##fname (char * sysfs_path, char * dev) \
{ \
	char attr_path[SYSFS_PATH_SIZE]; \
	char attr_buff[SYSFS_PATH_SIZE]; \
\
	if(safe_sprintf(attr_path, fmt, sysfs_path, dev)) \
		return 0; \
	if (0 > sysfs_read_attribute_value(attr_path, attr_buff, sizeof(attr_buff))) \
		return 0; \
\
	return strtoul(attr_buff, NULL, 0); \
}

declare_sysfs_get_val(size, "%s/block/%s/size");

