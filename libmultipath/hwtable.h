#ifndef _HWTABLE_H
#define _HWTABLE_H

#define ADDHWE(a, b, c, d, e) \
	hwe = zalloc (sizeof(struct hwentry)); \
	hwe->vendor = zalloc (SCSI_VENDOR_SIZE * sizeof(char)); \
	snprintf (hwe->vendor, SCSI_VENDOR_SIZE, "%-8s", b); \
	hwe->product = zalloc (SCSI_PRODUCT_SIZE * sizeof(char)); \
	snprintf (hwe->product, SCSI_PRODUCT_SIZE, "%-17s", c); \
	hwe->pgpolicy = d; \
	hwe->getuid = e; \
	vector_alloc_slot(a); \
	vector_set_slot(a, hwe);

#define ADDHWE_EXT(a, b, c, d, e, f, g, i, j) \
	hwe = zalloc (sizeof(struct hwentry)); \
	hwe->vendor = zalloc (SCSI_VENDOR_SIZE * sizeof(char)); \
	snprintf (hwe->vendor, SCSI_VENDOR_SIZE, "%-8s", b); \
	hwe->product = zalloc (SCSI_PRODUCT_SIZE * sizeof(char)); \
	snprintf (hwe->product, SCSI_PRODUCT_SIZE, "%-17s", c); \
	hwe->pgpolicy = d; \
	hwe->getuid = e; \
	hwe->getprio = f; \
	hwe->hwhandler = g; \
	hwe->features = i; \
	hwe->checker_index = get_checker_id(j); \
	vector_alloc_slot(a); \
	vector_set_slot(a, hwe);

struct hwentry {
        int selector_args;
        int pgpolicy;
        int checker_index;

        char * vendor;
        char * product;
        char * selector;
        char * getuid;
        char * getprio;
        char * features;
        char * hwhandler;
};

void setup_default_hwtable (vector hw);
struct hwentry * find_hwe (vector hwtable, char * vendor, char * product);

#endif /* _HWTABLE_H */
