#define SYSFS_PATH_SIZE 255
#define INQUIRY_CMDLEN  6
#define INQUIRY_CMD     0x12
#define SENSE_BUFF_LEN  32
#define DEF_TIMEOUT     60000
#define RECOVERED_ERROR 0x01
#define MX_ALLOC_LEN    255
#define TUR_CMD_LEN     6

#ifndef BLKGETSIZE
#define BLKGETSIZE      _IO(0x12,96)
#endif

/*
 * exerpt from sg_err.h
 */
#define SCSI_CHECK_CONDITION    0x2
#define SCSI_COMMAND_TERMINATED 0x22
#define SG_ERR_DRIVER_SENSE     0x08

int sysfs_get_vendor (char * sysfs_path, char * dev, char * buff, int len);
int sysfs_get_model (char * sysfs_path, char * dev, char * buff, int len);
int sysfs_get_rev (char * sysfs_path, char * dev, char * buff, int len);
int sysfs_get_dev (char * sysfs_path, char * dev, char * buff, int len);

unsigned long sysfs_get_size (char * sysfs_path, char * dev);
int path_discovery (vector pathvec, struct config * conf);

void basename (char *, char *);
int get_serial (char * buff, int fd);
int do_tur (char *);
int devt2devname (char *, char *);
int devinfo (struct path *, vector hwtable);

#if 0
int get_claimed(int fd);
#endif
