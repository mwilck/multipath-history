#define INQUIRY_CMDLEN  6
#define INQUIRY_CMD     0x12
#define SENSE_BUFF_LEN  32
#define DEF_TIMEOUT     60000
#define RECOVERED_ERROR 0x01
#define MX_ALLOC_LEN    255
#define BLKGETSIZE      _IO(0x12,96)
#define TUR_CMD_LEN     6

/* exerpt from "sg_err.h" */
#define SCSI_CHECK_CONDITION    0x2
#define SCSI_COMMAND_TERMINATED 0x22
#define SG_ERR_DRIVER_SENSE     0x08

#include "main.h"

void basename (char *, char *);
int get_serial (char *, char *);
int sysfs_devinfo (struct path *);
unsigned long get_disk_size (char *);
int do_tur (char *);
int get_claimed(char *);

/* internal getuid methods */
int get_evpd_wwid (char *, char *);
