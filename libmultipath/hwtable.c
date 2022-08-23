#include <stdio.h>

#include "checkers.h"
#include "vector.h"
#include "defaults.h"
#include "structs.h"
#include "config.h"
#include "pgpolicies.h"
#include "prio.h"

/*
 * Tuning suggestions on these parameters should go to
 * dm-devel@redhat.com
 * 
 * You are welcome to claim maintainership over a controller
 * family. Please mail the currently enlisted maintainer and
 * the upstream package maintainer.
 */
static struct hwentry default_hw[] = {
	/*
	 * Compellent Technologies, Inc.
	 *
	 * Maintainer : Jim Lester, Compellent
	 * Mail : jim.lester@compellent.com
	 */
	{
		.vendor        = "COMPELNT",
		.product       = "Compellent Vol",
		.getuid        = DEFAULT_GETUID,
		.features      = DEFAULT_FEATURES,
		.hwhandler     = DEFAULT_HWHANDLER,
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = MULTIBUS,
		.pgfailback    = -FAILBACK_IMMEDIATE,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = NO_PATH_RETRY_QUEUE,
		.minio         = DEFAULT_MINIO,
		.checker_name  = TUR,
		.prio_name     = DEFAULT_PRIO,
		.prio_args     = NULL,
	},
	/*
	 * Apple controller family
	 *
	 * Maintainer : Shyam Sundar
	 * Mail : g.shyamsundar@yahoo.co.in
	 */
	{
		.vendor        = "APPLE*",
		.product       = "Xserve RAID ",
		.getuid        = DEFAULT_GETUID,
		.features      = DEFAULT_FEATURES,
		.hwhandler     = DEFAULT_HWHANDLER,
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = MULTIBUS,
		.pgfailback    = FAILBACK_UNDEF,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = NO_PATH_RETRY_UNDEF,
		.minio         = DEFAULT_MINIO,
		.checker_name  = DEFAULT_CHECKER,
		.prio_name     = DEFAULT_PRIO,
		.prio_args     = NULL,
	},
	/*
	 * StorageWorks controller family
	 *
	 * Maintainer : Christophe Varoqui
	 * Mail : christophe.varoqui@opensvc.com
	 */
	{
		.vendor        = "3PARdata",
		.product       = "VV",
		.getuid        = DEFAULT_GETUID,
		.features      = DEFAULT_FEATURES,
		.hwhandler     = DEFAULT_HWHANDLER,
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = MULTIBUS,
		.pgfailback    = FAILBACK_UNDEF,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = NO_PATH_RETRY_UNDEF,
		.minio         = DEFAULT_MINIO,
		.checker_name  = DEFAULT_CHECKER,
		.prio_name     = DEFAULT_PRIO,
		.prio_args     = NULL,
	},
	{
		.vendor        = "DEC",
		.product       = "HSG80",
		.getuid        = DEFAULT_GETUID,
		.features      = "1 queue_if_no_path",
		.hwhandler     = "1 hp_sw",
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = GROUP_BY_PRIO,
		.pgfailback    = FAILBACK_UNDEF,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = NO_PATH_RETRY_UNDEF,
		.minio         = DEFAULT_MINIO,
		.checker_name  = HP_SW,
		.prio_name     = PRIO_HP_SW,
		.prio_args     = NULL,
	},
	{
		.vendor        = "HP",
		.product       = "A6189A",
		.getuid        = DEFAULT_GETUID,
		.features      = DEFAULT_FEATURES,
		.hwhandler     = DEFAULT_HWHANDLER,
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = MULTIBUS,
		.pgfailback    = FAILBACK_UNDEF,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = 12,
		.minio         = DEFAULT_MINIO,
		.checker_name  = DIRECTIO,
		.prio_name     = DEFAULT_PRIO,
		.prio_args     = NULL,
	},
	{
		/* MSA 1000/MSA1500 EVA 3000/5000 with old firmware */
		.vendor        = "(COMPAQ|HP)",
		.product       = "(MSA|HSV)1.0.*",
		.getuid        = DEFAULT_GETUID,
		.features      = "1 queue_if_no_path",
		.hwhandler     = "1 hp_sw",
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = GROUP_BY_PRIO,
		.pgfailback    = FAILBACK_UNDEF,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = 12,
		.minio         = 100,
		.checker_name  = HP_SW,
		.prio_name     = PRIO_HP_SW,
		.prio_args     = NULL,
	},
	{
		/* MSA 1000/1500 with new firmware */
		.vendor        = "(COMPAQ|HP)",
		.product       = "MSA VOLUME",
		.getuid        = DEFAULT_GETUID,
		.features      = DEFAULT_FEATURES,
		.hwhandler     = DEFAULT_HWHANDLER,
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = GROUP_BY_PRIO,
		.pgfailback    = -FAILBACK_IMMEDIATE,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = 12,
		.minio         = 100,
		.checker_name  = TUR,
		.prio_name     = PRIO_ALUA,
		.prio_args     = NULL,
	},
	{
		/* EVA 3000/5000 with new firmware, EVA 4000/6000/8000 */
		.vendor        = "(COMPAQ|HP)",
		.product       = "HSV1[01]1|HSV2[01]0|HSV300|HSV4[05]0",
		.getuid        = DEFAULT_GETUID,
		.features      = DEFAULT_FEATURES,
		.hwhandler     = DEFAULT_HWHANDLER,
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = GROUP_BY_PRIO,
		.pgfailback    = -FAILBACK_IMMEDIATE,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = 12,
		.minio         = 100,
		.checker_name  = TUR,
		.prio_name     = PRIO_ALUA,
		.prio_args     = NULL,
	},
	{
		/* HP MSA2000 product family with old firmware */
		.vendor        = "HP",
		.product       = "MSA2[02]12fc|MSA2012i",
		.getuid        = DEFAULT_GETUID,
		.features      = DEFAULT_FEATURES,
		.hwhandler     = DEFAULT_HWHANDLER,
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = MULTIBUS,
		.pgfailback    = -FAILBACK_IMMEDIATE,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = 18,
		.minio         = 100,
		.checker_name  = TUR,
		.prio_name     = DEFAULT_PRIO,
		.prio_args     = NULL,
	},
	{
		/* HP MSA2000 product family with new firmware */
		.vendor        = "HP",
		.product       = "MSA2012sa|MSA23(12|24)(fc|i|sa)|MSA2000s VOLUME",
		.getuid        = DEFAULT_GETUID,
		.features      = DEFAULT_FEATURES,
		.hwhandler     = DEFAULT_HWHANDLER,
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = GROUP_BY_PRIO,
		.pgfailback    = -FAILBACK_IMMEDIATE,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = 18,
		.minio         = 100,
		.checker_name  = TUR,
		.prio_name     = PRIO_ALUA,
		.prio_args     = NULL,
	},

	{
		/* HP SVSP */
		.vendor        = "HP",
		.product       = "HSVX700",
		.getuid        = DEFAULT_GETUID,
		.features      = DEFAULT_FEATURES,
		.hwhandler     = "1 alua",
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = GROUP_BY_PRIO,
		.pgfailback    = -FAILBACK_IMMEDIATE,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = 12,
		.minio         = 100,
		.checker_name  = TUR,
		.prio_name     = PRIO_ALUA,
		.prio_args     = NULL,
	},

	{
		/* HP Smart Array */
		.vendor        = "HP",
		.product       = "LOGICAL VOLUME.*",
		.getuid        = DEFAULT_GETUID,
		.features      = DEFAULT_FEATURES,
		.hwhandler     = DEFAULT_HWHANDLER,
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = MULTIBUS,
		.pgfailback    = FAILBACK_UNDEF,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = 12,
		.minio         = DEFAULT_MINIO,
		.checker_name  = CCISS_TUR,
		.prio_name     = DEFAULT_PRIO,
		.prio_args     = NULL,
	},
	/*
	 * DDN controller family
	 *
	 * Maintainer : Christophe Varoqui
	 * Mail : christophe.varoqui@opensvc.com
	 */
	{
		.vendor        = "DDN",
		.product       = "SAN DataDirector",
		.getuid        = DEFAULT_GETUID,
		.features      = DEFAULT_FEATURES,
		.hwhandler     = DEFAULT_HWHANDLER,
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = MULTIBUS,
		.pgfailback    = FAILBACK_UNDEF,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = NO_PATH_RETRY_UNDEF,
		.minio         = DEFAULT_MINIO,
		.checker_name  = DIRECTIO,
		.prio_name     = DEFAULT_PRIO,
		.prio_args     = NULL,
	},
	/*
	 * EMC / Clariion controller family
	 *
	 * Maintainer : Edward Goggin, EMC
	 * Mail : egoggin@emc.com
	 */
	{
		.vendor        = "EMC",
		.product       = "SYMMETRIX",
		.getuid        = "/lib/udev/scsi_id --page=pre-spc3-83 --whitelisted --device=/dev/%n",
		.features      = DEFAULT_FEATURES,
		.hwhandler     = DEFAULT_HWHANDLER,
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = MULTIBUS,
		.pgfailback    = FAILBACK_UNDEF,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = NO_PATH_RETRY_UNDEF,
		.minio         = DEFAULT_MINIO,
		.checker_name  = DIRECTIO,
		.prio_name     = DEFAULT_PRIO,
		.prio_args     = NULL,
	},
	{
		.vendor        = "DGC",
		.product       = ".*",
		.bl_product    = "LUNZ",
		.getuid        = DEFAULT_GETUID,
		.features      = "1 queue_if_no_path",
		.hwhandler     = "1 emc",
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = GROUP_BY_PRIO,
		.pgfailback    = -FAILBACK_IMMEDIATE,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = (300 / DEFAULT_CHECKINT),
		.minio         = DEFAULT_MINIO,
		.checker_name  = EMC_CLARIION,
		.prio_name     = PRIO_EMC,
		.prio_args     = NULL,
	},
	{
		.vendor        = "EMC",
		.product       = "Invista",
		.bl_product    = "LUNZ",
		.getuid        = DEFAULT_GETUID,
		.features      = DEFAULT_FEATURES,
		.hwhandler     = DEFAULT_HWHANDLER,
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = MULTIBUS,
		.pgfailback    = FAILBACK_UNDEF,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = 5,
		.minio         = DEFAULT_MINIO,
		.prio_name     = DEFAULT_PRIO,
		.prio_args     = NULL,
	},
	/*
	 * Fujitsu controller family
	 *
	 * Maintainer : Christophe Varoqui
	 * Mail : christophe.varoqui@opensvc.com
	 */
	{
		.vendor        = "FSC",
		.product       = "CentricStor",
		.getuid        = DEFAULT_GETUID,
		.features      = DEFAULT_FEATURES,
		.hwhandler     = DEFAULT_HWHANDLER,
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = GROUP_BY_SERIAL,
		.pgfailback    = FAILBACK_UNDEF,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = NO_PATH_RETRY_UNDEF,
		.minio         = DEFAULT_MINIO,
		.checker_name  = DIRECTIO,
		.prio_name     = DEFAULT_PRIO,
		.prio_args     = NULL,
	},
	{
		.vendor        = "EMC",
		.product       = "Invista",
		.bl_product    = "LUNZ",
		.getuid        = DEFAULT_GETUID,
		.features      = DEFAULT_FEATURES,
		.hwhandler     = DEFAULT_HWHANDLER,
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = FAILOVER,
		.pgfailback    = FAILBACK_UNDEF,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = 5,
		.minio         = DEFAULT_MINIO,
		.checker_name  = TUR,
		.prio_name     = DEFAULT_PRIO,
		.prio_args     = NULL,
	},
	/*
	 * Hitachi controller family
	 *
	 * Maintainer : Matthias Rudolph
	 * Mail : matthias.rudolph@hds.com
	 */
	{
		.vendor        = "(HITACHI|HP)",
		.product       = "OPEN-.*",
		.getuid        = DEFAULT_GETUID,
		.features      = DEFAULT_FEATURES,
		.hwhandler     = DEFAULT_HWHANDLER,
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = MULTIBUS,
		.pgfailback    = FAILBACK_UNDEF,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = NO_PATH_RETRY_UNDEF,
		.minio         = DEFAULT_MINIO,
		.checker_name  = TUR,
		.prio_name     = DEFAULT_PRIO,
		.prio_args     = NULL,
	},
	{
		.vendor        = "HITACHI",
		.product       = "DF.*",
		.getuid        = DEFAULT_GETUID,
		.features      = "1 queue_if_no_path",
		.hwhandler     = DEFAULT_HWHANDLER,
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = GROUP_BY_PRIO,
		.pgfailback    = -FAILBACK_IMMEDIATE,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = NO_PATH_RETRY_UNDEF,
		.minio         = DEFAULT_MINIO,
		.checker_name  = TUR,
		.prio_name     = PRIO_HDS,
		.prio_args     = NULL,
	},
	/*
	 * IBM controller family
	 *
	 * Maintainer : Hannes Reinecke, SuSE
	 * Mail : hare@suse.de
	 */
	{
		.vendor        = "IBM",
		.product       = "ProFibre 4000R",
		.getuid        = DEFAULT_GETUID,
		.features      = DEFAULT_FEATURES,
		.hwhandler     = DEFAULT_HWHANDLER,
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = MULTIBUS,
		.pgfailback    = FAILBACK_UNDEF,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = NO_PATH_RETRY_UNDEF,
		.minio         = DEFAULT_MINIO,
		.checker_name  = DIRECTIO,
		.prio_name     = DEFAULT_PRIO,
		.prio_args     = NULL,
	},
	{
		/* IBM FAStT 1722-600 */
		.vendor        = "IBM",
		.product       = "1722-600",
		.getuid        = DEFAULT_GETUID,
		.features      = "1 queue_if_no_path",
		.hwhandler     = "1 rdac",
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = GROUP_BY_PRIO,
		.pgfailback    = -FAILBACK_IMMEDIATE,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = 300,
		.minio         = DEFAULT_MINIO,
		.checker_name  = RDAC,
		.prio_name     = PRIO_RDAC,
		.prio_args     = NULL,
	},
	{
		/* IBM DS4100 */
		.vendor        = "IBM",
		.product       = "1724",
		.getuid        = DEFAULT_GETUID,
		.features      = "1 queue_if_no_path",
		.hwhandler     = "1 rdac",
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = GROUP_BY_PRIO,
		.pgfailback    = -FAILBACK_IMMEDIATE,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = 300,
		.minio         = DEFAULT_MINIO,
		.checker_name  = RDAC,
		.prio_name     = PRIO_RDAC,
		.prio_args     = NULL,
	},
	{
		/* IBM DS3200 / DS3300 / DS3400 */
		.vendor        = "IBM",
		.product       = "1726",
		.getuid        = DEFAULT_GETUID,
		.features      = "1 queue_if_no_path",
		.hwhandler     = "1 rdac",
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = GROUP_BY_PRIO,
		.pgfailback    = -FAILBACK_IMMEDIATE,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = 300,
		.minio         = DEFAULT_MINIO,
		.checker_name  = RDAC,
		.prio_name     = PRIO_RDAC,
		.prio_args     = NULL,
	},
	{
		/* IBM DS4400 / DS4500 / FAStT700 */
		.vendor        = "IBM",
		.product       = "1742",
		.getuid        = DEFAULT_GETUID,
		.features      = DEFAULT_FEATURES,
		.hwhandler     = "1 rdac",
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = GROUP_BY_PRIO,
		.pgfailback    = -FAILBACK_IMMEDIATE,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = NO_PATH_RETRY_QUEUE,
		.minio         = DEFAULT_MINIO,
		.checker_name  = RDAC,
		.prio_name     = PRIO_RDAC,
		.prio_args     = NULL,
	},
	{
		.vendor        = "IBM",
		.product       = "1745|1746",
		.getuid        = DEFAULT_GETUID,
		.features      = "2 pg_init_retries 50",
		.hwhandler     = "1 rdac",
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = GROUP_BY_PRIO,
		.pgfailback    = -FAILBACK_IMMEDIATE,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = 15,
		.minio         = DEFAULT_MINIO,
		.checker_name  = RDAC,
		.prio_name     = PRIO_RDAC,
		.prio_args     = NULL,
	},
	{
	    /* IBM DS4700 */
		.vendor        = "IBM",
		.product       = "1814",
		.getuid        = DEFAULT_GETUID,
		.features      = DEFAULT_FEATURES,
		.hwhandler     = "1 rdac",
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = GROUP_BY_PRIO,
		.pgfailback    = -FAILBACK_IMMEDIATE,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = NO_PATH_RETRY_QUEUE,
		.minio         = DEFAULT_MINIO,
		.checker_name  = RDAC,
		.prio_name     = PRIO_RDAC,
		.prio_args     = NULL,
	},
	{
	    /* IBM DS4800 */
		.vendor        = "IBM",
		.product       = "1815",
		.getuid        = DEFAULT_GETUID,
		.features      = DEFAULT_FEATURES,
		.hwhandler     = "1 rdac",
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = GROUP_BY_PRIO,
		.pgfailback    = -FAILBACK_IMMEDIATE,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = NO_PATH_RETRY_QUEUE,
		.minio         = DEFAULT_MINIO,
		.checker_name  = RDAC,
		.prio_name     = PRIO_RDAC,
		.prio_args     = NULL,
	},
	{
	    /* IBM DS5000 */
		.vendor        = "IBM",
		.product       = "1818",
		.getuid        = DEFAULT_GETUID,
		.features      = DEFAULT_FEATURES,
		.hwhandler     = "1 rdac",
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = GROUP_BY_PRIO,
		.pgfailback    = -FAILBACK_IMMEDIATE,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = NO_PATH_RETRY_QUEUE,
		.minio         = DEFAULT_MINIO,
		.checker_name  = RDAC,
		.prio_name     = PRIO_RDAC,
		.prio_args     = NULL,
	},
	{
		/* IBM Netfinity Fibre Channel RAID Controller Unit */
		.vendor        = "IBM",
		.product       = "3526",
		.getuid        = DEFAULT_GETUID,
		.features      = DEFAULT_FEATURES,
		.hwhandler     = "1 rdac",
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = GROUP_BY_PRIO,
		.pgfailback    = -FAILBACK_IMMEDIATE,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = NO_PATH_RETRY_QUEUE,
		.minio         = DEFAULT_MINIO,
		.checker_name  = RDAC,
		.prio_name     = PRIO_RDAC,
		.prio_args     = NULL,
	},
	{
		/* IBM DS4200 / FAStT200 */
		.vendor        = "IBM",
		.product       = "3542",
		.getuid        = DEFAULT_GETUID,
		.features      = DEFAULT_FEATURES,
		.hwhandler     = DEFAULT_HWHANDLER,
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = GROUP_BY_SERIAL,
		.pgfailback    = FAILBACK_UNDEF,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = NO_PATH_RETRY_UNDEF,
		.minio         = DEFAULT_MINIO,
		.checker_name  = TUR,
		.prio_name     = DEFAULT_PRIO,
		.prio_args     = NULL,
	},
	{
		/* IBM ESS F20 aka Shark */
		.vendor        = "IBM",
		.product       = "2105800",
		.getuid        = DEFAULT_GETUID,
		.features      = "1 queue_if_no_path",
		.hwhandler     = DEFAULT_HWHANDLER,
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = GROUP_BY_SERIAL,
		.pgfailback    = FAILBACK_UNDEF,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = NO_PATH_RETRY_UNDEF,
		.minio         = DEFAULT_MINIO,
		.checker_name  = TUR,
		.prio_name     = DEFAULT_PRIO,
		.prio_args     = NULL,
	},
	{
		/* IBM ESS F20 aka Shark */
		.vendor        = "IBM",
		.product       = "2105F20",
		.getuid        = DEFAULT_GETUID,
		.features      = "1 queue_if_no_path",
		.hwhandler     = DEFAULT_HWHANDLER,
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = GROUP_BY_SERIAL,
		.pgfailback    = FAILBACK_UNDEF,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = NO_PATH_RETRY_UNDEF,
		.minio         = DEFAULT_MINIO,
		.checker_name  = TUR,
		.prio_name     = DEFAULT_PRIO,
		.prio_args     = NULL,
	},
	{
		/* IBM DS6000 */
		.vendor        = "IBM",
		.product       = "1750500",
		.getuid        = DEFAULT_GETUID,
		.features      = "1 queue_if_no_path",
		.hwhandler     = DEFAULT_HWHANDLER,
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = GROUP_BY_PRIO,
		.pgfailback    = -FAILBACK_IMMEDIATE,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = NO_PATH_RETRY_UNDEF,
		.minio         = DEFAULT_MINIO,
		.checker_name  = TUR,
		.prio_name     = PRIO_ALUA,
		.prio_args     = NULL,
	},
	{
		/* IBM DS8000 */
		.vendor        = "IBM",
		.product       = "2107900",
		.getuid        = DEFAULT_GETUID,
		.features      = "1 queue_if_no_path",
		.hwhandler     = DEFAULT_HWHANDLER,
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = MULTIBUS,
		.pgfailback    = FAILBACK_UNDEF,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = NO_PATH_RETRY_UNDEF,
		.minio         = DEFAULT_MINIO,
		.checker_name  = TUR,
		.prio_name     = DEFAULT_PRIO,
		.prio_args     = NULL,
	},
	{
		/* IBM SAN Volume Controller */
		.vendor        = "IBM",
		.product       = "2145",
		.getuid        = DEFAULT_GETUID,
		.features      = "1 queue_if_no_path",
		.hwhandler     = DEFAULT_HWHANDLER,
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = GROUP_BY_PRIO,
		.pgfailback    = -FAILBACK_IMMEDIATE,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = NO_PATH_RETRY_UNDEF,
		.minio         = DEFAULT_MINIO,
		.checker_name  = TUR,
		.prio_name     = PRIO_ALUA,
		.prio_args     = NULL,
	},
	{
		/* IBM S/390 ECKD DASD */
		.vendor        = "IBM",
		.product       = "S/390 DASD ECKD",
		.bl_product       = "S/390.*",
		.getuid        = "/sbin/dasdinfo -u -b %n",
		.features      = "1 queue_if_no_path",
		.hwhandler     = DEFAULT_HWHANDLER,
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = MULTIBUS,
		.pgfailback    = FAILBACK_UNDEF,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = NO_PATH_RETRY_UNDEF,
		.minio         = DEFAULT_MINIO,
		.checker_name  = DIRECTIO,
		.prio_name     = DEFAULT_PRIO,
		.prio_args     = NULL,
	},
	{
		/* IBM S/390 FBA DASD */
		.vendor        = "IBM",
		.product       = "S/390 DASD FBA",
		.bl_product       = "S/390.*",
		.getuid        = "/sbin/dasdinfo -u -b %n",
		.features      = "1 queue_if_no_path",
		.hwhandler     = DEFAULT_HWHANDLER,
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = MULTIBUS,
		.pgfailback    = FAILBACK_UNDEF,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = NO_PATH_RETRY_UNDEF,
		.minio         = DEFAULT_MINIO,
		.checker_name  = DIRECTIO,
		.prio_name     = DEFAULT_PRIO,
		.prio_args     = NULL,
	},
	{
		/* IBM IPR */
		.vendor        = "IBM",
		.product       = "IPR.*",
		.getuid        = DEFAULT_GETUID,
		.features      = "1 queue_if_no_path",
		.hwhandler     = "1 alua",
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = GROUP_BY_PRIO,
		.pgfailback    = -FAILBACK_IMMEDIATE,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = NO_PATH_RETRY_UNDEF,
		.minio         = DEFAULT_MINIO,
		.checker_name  = TUR,
		.prio_name     = PRIO_ALUA,
		.prio_args     = NULL,
	},
	/*
	 * IBM Power Virtual SCSI Devices
	 *
	 * Maintainer : Brian King, IBM
	 * Mail : brking@linux.vnet.ibm.com
	 */
	{
		/* AIX VDASD */
		.vendor        = "AIX",
		.product       = "VDASD",
		.getuid        = DEFAULT_GETUID,
		.features      = DEFAULT_FEATURES,
		.hwhandler     = DEFAULT_HWHANDLER,
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = MULTIBUS,
		.pgfailback    = -FAILBACK_IMMEDIATE,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = (300 / DEFAULT_CHECKINT),
		.minio         = DEFAULT_MINIO,
		.checker_name  = DIRECTIO,
		.prio_name     = DEFAULT_PRIO,
		.prio_args     = NULL,
	},
	{
		/* IBM 3303      NVDISK */
		.vendor        = "IBM",
		.product       = "3303      NVDISK",
		.getuid        = DEFAULT_GETUID,
		.features      = DEFAULT_FEATURES,
		.hwhandler     = DEFAULT_HWHANDLER,
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = FAILOVER,
		.pgfailback    = -FAILBACK_IMMEDIATE,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = (300 / DEFAULT_CHECKINT),
		.minio         = DEFAULT_MINIO,
		.checker_name  = TUR,
		.prio_name     = DEFAULT_PRIO,
		.prio_args     = NULL,
	},

	{
		/* DELL MD3000 */
		.vendor        = "DELL",
		.product       = "MD3000",
		.getuid        = DEFAULT_GETUID,
		.features      = "2 pg_init_retries 50",
		.hwhandler     = "1 rdac",
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = GROUP_BY_PRIO,
		.pgfailback    = -FAILBACK_IMMEDIATE,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = 15,
		.minio         = DEFAULT_MINIO,
		.checker_name  = RDAC,
		.prio_name     = PRIO_RDAC,
		.prio_args     = NULL,
	},
	{
		/* DELL MD3000i */
		.vendor        = "DELL",
		.product       = "MD3000i",
		.getuid        = DEFAULT_GETUID,
		.features      = "2 pg_init_retries 50",
		.hwhandler     = "1 rdac",
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = GROUP_BY_PRIO,
		.pgfailback    = -FAILBACK_IMMEDIATE,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = 15,
		.minio         = DEFAULT_MINIO,
		.checker_name  = RDAC,
		.prio_name     = PRIO_RDAC,
		.prio_args     = NULL,
	},
	{
		/* DELL MD32xx */
		.vendor        = "DELL",
		.product       = "MD32xx",
		.getuid        = DEFAULT_GETUID,
		.features      = "2 pg_init_retries 50",
		.hwhandler     = "1 rdac",
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = GROUP_BY_PRIO,
		.pgfailback    = -FAILBACK_IMMEDIATE,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = 15,
		.minio         = DEFAULT_MINIO,
		.checker_name  = RDAC,
		.prio_name     = PRIO_RDAC,
		.prio_args     = NULL,
	},
	{
		/* DELL MD32xxi */
		.vendor        = "DELL",
		.product       = "MD32xxi",
		.getuid        = DEFAULT_GETUID,
		.features      = "2 pg_init_retries 50",
		.hwhandler     = "1 rdac",
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = GROUP_BY_PRIO,
		.pgfailback    = -FAILBACK_IMMEDIATE,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = 15,
		.minio         = DEFAULT_MINIO,
		.checker_name  = RDAC,
		.prio_name     = PRIO_RDAC,
		.prio_args     = NULL,
	},
	/*
	 * NETAPP controller family
	 *
	 * Maintainer : Dave Wysochanski
	 * Mail : davidw@netapp.com
	 */
	{
		.vendor        = "NETAPP",
		.product       = "LUN.*",
		.getuid        = DEFAULT_GETUID,
		.features      = "1 queue_if_no_path",
		.hwhandler     = DEFAULT_HWHANDLER,
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = GROUP_BY_PRIO,
		.pgfailback    = -FAILBACK_IMMEDIATE,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = NO_PATH_RETRY_UNDEF,
		.minio         = 128,
		.checker_name  = DIRECTIO,
		.prio_name     = PRIO_NETAPP,
		.prio_args     = NULL,
	},
	/*
	 * NEXENTA/COMSTAR controller family
	 *
	 * Maintainer : Yacine Kheddache
	 * Mail : yacine@alyseo.com
	 */
	{
		.vendor        = "NEXENTA",
		.product       = "COMSTAR",
		.getuid        = DEFAULT_GETUID,
		.features      = "1 queue_if_no_path",
		.hwhandler     = DEFAULT_HWHANDLER,
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = GROUP_BY_SERIAL,
		.pgfailback    = -FAILBACK_IMMEDIATE,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = 30,
		.minio         = 128,
		.checker_name  = DIRECTIO,
		.prio_name     = DEFAULT_PRIO,
		.prio_args     = NULL,
	},
	/*
	 * IBM NSeries (NETAPP) controller family
	 *
	 * Maintainer : Dave Wysochanski
	 * Mail : davidw@netapp.com
	 */
	{
		.vendor        = "IBM",
		.product       = "Nseries.*",
		.getuid        = DEFAULT_GETUID,
		.features      = "1 queue_if_no_path",
		.hwhandler     = DEFAULT_HWHANDLER,
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = GROUP_BY_PRIO,
		.pgfailback    = -FAILBACK_IMMEDIATE,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = NO_PATH_RETRY_UNDEF,
		.minio         = 128,
		.checker_name  = DIRECTIO,
		.prio_name     = PRIO_NETAPP,
		.prio_args     = NULL,
	},
	/*
	 * Pillar Data controller family
	 *
	 * Maintainer : Srinivasan Ramani
	 * Mail : sramani@pillardata.com
	 */
	{
		.vendor        = "Pillar",
		.product       = "Axiom.*",
		.getuid        = DEFAULT_GETUID,
		.features      = DEFAULT_FEATURES,
		.hwhandler     = DEFAULT_HWHANDLER,
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = GROUP_BY_PRIO,
		.pgfailback    = FAILBACK_UNDEF,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = NO_PATH_RETRY_UNDEF,
		.minio         = DEFAULT_MINIO,
		.checker_name  = TUR,
		.prio_name     = PRIO_ALUA,
		.prio_args     = NULL,
	},
	/*
	 * SGI arrays
	 *
	 * Maintainer : Christophe Varoqui
	 * Mail : christophe.varoqui@opensvc.com
	 */
	{
		.vendor        = "SGI",
		.product       = "TP9[13]00",
		.getuid        = DEFAULT_GETUID,
		.features      = DEFAULT_FEATURES,
		.hwhandler     = DEFAULT_HWHANDLER,
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = MULTIBUS,
		.pgfailback    = FAILBACK_UNDEF,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = NO_PATH_RETRY_UNDEF,
		.minio         = DEFAULT_MINIO,
		.checker_name  = DIRECTIO,
		.prio_name     = DEFAULT_PRIO,
		.prio_args     = NULL,
	},
	{
		.vendor        = "SGI",
		.product       = "TP9[45]00",
		.getuid        = DEFAULT_GETUID,
		.features      = DEFAULT_FEATURES,
		.hwhandler     = "1 rdac",
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = GROUP_BY_PRIO,
		.pgfailback    = -FAILBACK_IMMEDIATE,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = NO_PATH_RETRY_QUEUE,
		.minio         = DEFAULT_MINIO,
		.checker_name  = RDAC,
		.prio_name     = PRIO_RDAC,
		.prio_args     = NULL,
	},
	{
		.vendor        = "SGI",
		.product       = "IS.*",
		.getuid        = DEFAULT_GETUID,
		.features      = DEFAULT_FEATURES,
		.hwhandler     = "1 rdac",
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = GROUP_BY_PRIO,
		.pgfailback    = -FAILBACK_IMMEDIATE,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = NO_PATH_RETRY_QUEUE,
		.minio         = DEFAULT_MINIO,
		.checker_name  = RDAC,
		.prio_name     = PRIO_RDAC,
		.prio_args     = NULL,
	},
	/*
	 * STK arrays
	 *
	 * Maintainer : Christophe Varoqui
	 * Mail : christophe.varoqui@opensvc.com
	 */
	{
		.vendor        = "STK",
		.product       = "OPENstorage D280",
		.getuid        = DEFAULT_GETUID,
		.features      = DEFAULT_FEATURES,
		.hwhandler     = "1 rdac",
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = GROUP_BY_PRIO,
		.pgfailback    = -FAILBACK_IMMEDIATE,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = NO_PATH_RETRY_UNDEF,
		.minio         = DEFAULT_MINIO,
		.checker_name  = TUR,
		.prio_name     = PRIO_RDAC,
		.prio_args     = NULL,
	},
	/*
	 * SUN arrays
	 *
	 * Maintainer : Christophe Varoqui
	 * Mail : christophe.varoqui@opensvc.com
	 */
	{
		.vendor        = "SUN",
		.product       = "(StorEdge 3510|T4)",
		.getuid        = DEFAULT_GETUID,
		.features      = DEFAULT_FEATURES,
		.hwhandler     = DEFAULT_HWHANDLER,
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = MULTIBUS,
		.pgfailback    = FAILBACK_UNDEF,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = NO_PATH_RETRY_UNDEF,
		.minio         = DEFAULT_MINIO,
		.checker_name  = DIRECTIO,
		.prio_name     = DEFAULT_PRIO,
		.prio_args     = NULL,
	},
	/*
	 * Pivot3 RAIGE
	 *
	 * Maintainer : Bart Brooks, Pivot3
	 * Mail : bartb@pivot3.com
	 */
	{
		.vendor        = "PIVOT3",
		.product       = "RAIGE VOLUME",
		.getuid        = "/lib/udev/scsi_id --page=0x80 --whitelisted --device=/dev/%n",
		.features      = "1 queue_if_no_path",
		.hwhandler     = DEFAULT_HWHANDLER,
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = MULTIBUS,
		.pgfailback    = FAILBACK_UNDEF,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = NO_PATH_RETRY_UNDEF,
		.minio         = 100,
		.checker_name  = TUR,
		.prio_name     = DEFAULT_PRIO,
		.prio_args     = NULL,
	},
	{
		.vendor        = "SUN",
		.product       = "CSM200_R",
		.getuid        = DEFAULT_GETUID,
		.features      = DEFAULT_FEATURES,
		.hwhandler     = "1 rdac",
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = GROUP_BY_PRIO,
		.pgfailback    = -FAILBACK_IMMEDIATE,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = NO_PATH_RETRY_QUEUE,
		.minio         = DEFAULT_MINIO,
		.checker_name  = RDAC,
		.prio_name     = PRIO_RDAC,
		.prio_args     = NULL,
	},
	/* SUN/LSI 2510, 2540, 2530, 2540 */
	{
		.vendor        = "SUN",
		.product       = "LCSM100_[IEFS]",
		.getuid        = DEFAULT_GETUID,
		.features      = DEFAULT_FEATURES,
		.hwhandler     = "1 rdac",
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = GROUP_BY_PRIO,
		.pgfailback    = -FAILBACK_IMMEDIATE,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = NO_PATH_RETRY_QUEUE,
		.minio         = DEFAULT_MINIO,
		.checker_name  = RDAC,
		.prio_name     = PRIO_RDAC,
		.prio_args     = NULL,
	},
	/* LSI Generic rdac storage */
	{
		.vendor        = "(LSI|ENGENIO)",
		.product       = "INF-01-00",
		.getuid        = DEFAULT_GETUID,
		.features      = "2 pg_init_retries 50",
		.hwhandler     = "1 rdac",
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = GROUP_BY_PRIO,
		.pgfailback    = -FAILBACK_IMMEDIATE,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = 15,
		.minio         = DEFAULT_MINIO,
		.checker_name  = RDAC,
		.prio_name     = PRIO_RDAC,
		.prio_args     = NULL,
	},
	{
		.vendor        = "STK",
		.product       = "FLEXLINE 380",
		.bl_product    = "Universal Xport",
		.getuid        = DEFAULT_GETUID,
		.features      = DEFAULT_FEATURES,
		.hwhandler     = "1 rdac",
		.selector      = DEFAULT_SELECTOR,
		.pgpolicy      = GROUP_BY_PRIO,
		.pgfailback    = -FAILBACK_IMMEDIATE,
		.rr_weight     = RR_WEIGHT_NONE,
		.no_path_retry = NO_PATH_RETRY_QUEUE,
		.minio         = DEFAULT_MINIO,
		.checker_name  = RDAC,
		.prio_name     = PRIO_RDAC,
		.prio_args     = NULL,
	},
	/*
	 * EOL
	 */
	{
		.vendor        = NULL,
		.product       = NULL,
		.getuid        = NULL,
		.features      = NULL,
		.hwhandler     = NULL,
		.selector      = NULL,
		.pgpolicy      = 0,
		.pgfailback    = 0,
		.rr_weight     = 0,
		.no_path_retry = 0,
		.minio         = 0,
		.checker_name  = NULL,
		.prio_name     = NULL,
		.prio_args     = NULL,
	},
};

extern int
setup_default_hwtable (vector hw)
{
	int r = 0;
	struct hwentry * hwe = default_hw;

	while (hwe->vendor) {
		r += store_hwe(hw, hwe);
		hwe++;
	}
	return r;
}
