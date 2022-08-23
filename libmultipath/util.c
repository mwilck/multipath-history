#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <pthread.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>

#include "debug.h"
#include "memory.h"
#include "checkers.h"
#include "vector.h"
#include "structs.h"
#include "log.h"

size_t
strchop(char *str)
{
	int i;

	for (i=strlen(str)-1; i >=0 && isspace(str[i]); --i) ;
	str[++i] = '\0';
	return strlen(str);
}

int
basenamecpy (const char * str1, char * str2, int str2len)
{
	char *p;

	if (!str1 || !strlen(str1))
		return 0;

	if (strlen(str1) >= str2len)
		return 0;

	if (!str2)
		return 0;

	p = (char *)str1 + (strlen(str1) - 1);

	while (*--p != '/' && p != str1)
		continue;

	if (p != str1)
		p++;

	strncpy(str2, p, str2len);
	str2[str2len - 1] = '\0';
	return strchop(str2);
}

int
filepresent (char * run) {
	struct stat buf;

	if(!stat(run, &buf))
		return 1;
	return 0;
}

int
get_word (char * sentence, char ** word)
{
	char * p;
	int len;
	int skip = 0;

	if (word)
		*word = NULL;

	while (*sentence ==  ' ') {
		sentence++;
		skip++;
	}
	if (*sentence == '\0')
		return 0;

	p = sentence;

	while (*p !=  ' ' && *p != '\0')
		p++;

	len = (int) (p - sentence);

	if (!word)
		return skip + len;

	*word = MALLOC(len + 1);

	if (!*word) {
		condlog(0, "get_word : oom");
		return 0;
	}
	strncpy(*word, sentence, len);
	strchop(*word);
	condlog(4, "*word = %s, len = %i", *word, len);

	if (*p == '\0')
		return 0;

	return skip + len;
}

size_t strlcpy(char *dst, const char *src, size_t size)
{
	size_t bytes = 0;
	char *q = dst;
	const char *p = src;
	char ch;

	while ((ch = *p++)) {
		if (bytes+1 < size)
			*q++ = ch;
		bytes++;
	}

	/* If size == 0 there is no space for a final null... */
	if (size)
		*q = '\0';
	return bytes;
}

size_t strlcat(char *dst, const char *src, size_t size)
{
	size_t bytes = 0;
	char *q = dst;
	const char *p = src;
	char ch;

	while (bytes < size && *q) {
		q++;
		bytes++;
	}
	if (bytes == size)
		return (bytes + strlen(src));

	while ((ch = *p++)) {
		if (bytes+1 < size)
		*q++ = ch;
		bytes++;
	}

	*q = '\0';
	return bytes;
}

int devt2devname(char *devname, int devname_len, char *devt)
{
	FILE *fd;
	unsigned int tmpmaj, tmpmin, major, minor;
	char dev[FILE_NAME_SIZE];
	char block_path[PATH_SIZE];
	struct stat statbuf;

	memset(block_path, 0, sizeof(block_path));
	memset(dev, 0, sizeof(dev));
	if (sscanf(devt, "%u:%u", &major, &minor) != 2) {
		condlog(0, "Invalid device number %s", devt);
		return 1;
	}

	if (devname_len > FILE_NAME_SIZE)
		devname_len = FILE_NAME_SIZE;

	if (stat("/sys/dev/block", &statbuf) == 0) {
		/* Newer kernels have /sys/dev/block */
		sprintf(block_path,"/sys/dev/block/%u:%u", major, minor);
		if (lstat(block_path, &statbuf) == 0) {
			if (S_ISLNK(statbuf.st_mode) &&
			    readlink(block_path, dev, FILE_NAME_SIZE-1) > 0) {
				char *p = strrchr(dev, '/');

				if (!p) {
					condlog(0, "No sysfs entry for %s",
						block_path);
					return 1;
				}
				p++;
				strncpy(devname, p, devname_len);
				return 0;
			}
		}
		goto skip_proc;
	}
	memset(block_path, 0, sizeof(block_path));

	if (!(fd = fopen("/proc/partitions", "r"))) {
		condlog(0, "Cannot open /proc/partitions");
		return 1;
	}

	while (!feof(fd)) {
		int r = fscanf(fd,"%u %u %*d %s",&tmpmaj, &tmpmin, dev);
		if (!r) {
			r = fscanf(fd,"%*s\n");
			continue;
		}
		if (r != 3)
			continue;

		if ((major == tmpmaj) && (minor == tmpmin)) {
			if (snprintf(block_path, sizeof(block_path),
				     "/sys/block/%s", dev) >= sizeof(block_path)) {
				condlog(0, "device name %s is too long", dev);
				fclose(fd);
				return 1;
			}
			break;
		}
	}
	fclose(fd);
skip_proc:
	if (strncmp(block_path,"/sys/block", 10)) {
		condlog(3, "No device found for %u:%u", major, minor);
		return 1;
	}

	if (stat(block_path, &statbuf) < 0) {
		condlog(0, "No sysfs entry for %s", block_path);
		return 1;
	}

	if (S_ISDIR(statbuf.st_mode) == 0) {
		condlog(0, "sysfs entry %s is not a directory", block_path);
		return 1;
	}
	basenamecpy((const char *)block_path, devname, devname_len);
	return 0;
}

/* This function returns a pointer inside of the supplied pathname string.
 * If is_path_device is true, it may also modify the supplied string */
char *convert_dev(char *name, int is_path_device)
{
	char *ptr;

	if (!name)
		return NULL;
	if (is_path_device) {
		ptr = strstr(name, "cciss/");
		if (ptr) {
			ptr += 5;
			*ptr = '!';
		}
	}
	if (!strncmp(name, "/dev/", 5) && strlen(name) > 5)
		ptr = name + 5;
	else
		ptr = name;
	return ptr;
}

dev_t parse_devt(const char *dev_t)
{
	int maj, min;

	if (sscanf(dev_t,"%d:%d", &maj, &min) != 2)
		return 0;

	return makedev(maj, min);
}

char *parse_uid_attribute_by_attrs(char *uid_attrs, char *path_dev)
{
	char *uid_attribute;
	char *uid_attr_record;
	char *dev;
	char *attr;
	char *tmp;
	int  count;

	if(!uid_attrs || !path_dev)
		return NULL;

	count = get_word(uid_attrs, &uid_attr_record);
	while (uid_attr_record) {
		tmp = strrchr(uid_attr_record, ':');
		if (!tmp) {
			free(uid_attr_record);
			if (!count)
				break;
			uid_attrs += count;
			count = get_word(uid_attrs, &uid_attr_record);
			continue;
		}
		dev = uid_attr_record;
		attr = tmp + 1;
		*tmp = '\0';

		if(!strncmp(path_dev, dev, strlen(dev))) {
			uid_attribute = STRDUP(attr);
			free(uid_attr_record);
			return uid_attribute;
		}

		free(uid_attr_record);
		if (!count)
			break;
		uid_attrs += count;
		count = get_word(uid_attrs, &uid_attr_record);
	}
	return NULL;
}

void
setup_thread_attr(pthread_attr_t *attr, size_t stacksize, int detached)
{
	int ret;

	ret = pthread_attr_init(attr);
	assert(ret == 0);
	if (stacksize < PTHREAD_STACK_MIN)
		stacksize = PTHREAD_STACK_MIN;
	ret = pthread_attr_setstacksize(attr, stacksize);
	assert(ret == 0);
	if (detached) {
		ret = pthread_attr_setdetachstate(attr,
						  PTHREAD_CREATE_DETACHED);
		assert(ret == 0);
	}
}

int systemd_service_enabled_in(const char *dev, const char *prefix)
{
	char path[PATH_SIZE], file[PATH_SIZE], service[PATH_SIZE];
	DIR *dirfd;
	struct dirent *d;
	int found = 0;

	snprintf(service, PATH_SIZE, "multipathd.service");
	snprintf(path, PATH_SIZE, "%s/systemd/system", prefix);
	condlog(3, "%s: checking for %s in %s", dev, service, path);

	dirfd = opendir(path);
	if (dirfd == NULL)
		return 0;

	while ((d = readdir(dirfd)) != NULL) {
		char *p;
		struct stat stbuf;

		if ((strcmp(d->d_name,".") == 0) ||
		    (strcmp(d->d_name,"..") == 0))
			continue;

		if (strlen(d->d_name) < 6)
			continue;

		p = d->d_name + strlen(d->d_name) - 6;
		if (strcmp(p, ".wants"))
			continue;
		snprintf(file, PATH_SIZE, "%s/%s/%s",
			 path, d->d_name, service);
		if (stat(file, &stbuf) == 0) {
			condlog(3, "%s: found %s", dev, file);
			found++;
			break;
		}
	}
	closedir(dirfd);

	return found;
}

int systemd_service_enabled(const char *dev)
{
	int found = 0;

	found = systemd_service_enabled_in(dev, "/etc");
	if (!found)
		found = systemd_service_enabled_in(dev, "/usr/lib");
	if (!found)
		found = systemd_service_enabled_in(dev, "/lib");
	if (!found)
		found = systemd_service_enabled_in(dev, "/run");
	return found;
}
