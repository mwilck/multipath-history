#include "vector.h"
#include "structs.h"
#include "devinfo.h"
#include "sysfs_devinfo.h"

int
get_lun_strings(char * vendor_id, char * product_id, char * rev, char * dev)
{
        if (sysfs_get_vendor(sysfs_path, dev, vendor_id, 8))
                return 1;

        if (sysfs_get_model(sysfs_path, dev, product_id, 16))
                return 1;

        if (sysfs_get_rev(sysfs_path, dev, rev, 4))
                return 1;

	return 0;
}
