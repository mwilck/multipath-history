#define SYSFS_PATH_SIZE 255

int sysfs_get_vendor (char * sysfs_path, char * dev, char * buff, int len);
int sysfs_get_model (char * sysfs_path, char * dev, char * buff, int len);
int sysfs_get_rev (char * sysfs_path, char * dev, char * buff, int len);
int sysfs_get_dev (char * sysfs_path, char * dev, char * buff, int len);

unsigned long sysfs_get_size (char * sysfs_path, char * dev);
