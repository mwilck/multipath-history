#include <stdio.h>
#include <ctype.h>

#include "memory.h"
#include "vector.h"
#include "structs.h"
#include "defaults.h"
#include "hwtable.h"
#include "util.h"

#include "../libcheckers/checkers.h"
#include "../multipath/pgpolicies.h"

extern struct hwentry *
find_hwe (vector hwtable, char * vendor, char * product)
{
	int i;
	struct hwentry * hwe;

	vector_foreach_slot (hwtable, hwe, i) {
		if (strcmp_chomp(hwe->vendor, vendor) == 0 &&
		    (hwe->product[0] == '*' ||
		    strcmp_chomp(hwe->product, product) == 0))
			return hwe;
	}
	return NULL;
}

void
setup_default_hwtable (vector hw)
{
	struct hwentry * hwe;
	ADDHWE(hw, "COMPAQ", "HSV110 (C)COMPAQ", MULTIBUS, DEFAULT_GETUID);
	ADDHWE(hw, "COMPAQ", "MSA1000", MULTIBUS, DEFAULT_GETUID);
	ADDHWE(hw, "COMPAQ", "MSA1000 VOLUME", MULTIBUS, DEFAULT_GETUID);
	ADDHWE(hw, "DEC", "HSG80", MULTIBUS, DEFAULT_GETUID);
	ADDHWE(hw, "HP", "HSV110", MULTIBUS, DEFAULT_GETUID);
	ADDHWE(hw, "HP", "A6189A", MULTIBUS, DEFAULT_GETUID);
	ADDHWE(hw, "HP", "OPEN-", MULTIBUS, DEFAULT_GETUID);
	ADDHWE(hw, "DDN", "SAN DataDirector", MULTIBUS, DEFAULT_GETUID);
	ADDHWE(hw, "EMC", "SYMMETRIX", MULTIBUS, DEFAULT_GETUID);
	ADDHWE(hw, "FSC", "CentricStor", MULTIBUS, DEFAULT_GETUID);
	ADDHWE(hw, "HITACHI", "DF400", MULTIBUS, DEFAULT_GETUID);
	ADDHWE(hw, "HITACHI", "DF500", MULTIBUS, DEFAULT_GETUID);
	ADDHWE(hw, "HITACHI", "DF600", MULTIBUS, DEFAULT_GETUID);
	ADDHWE(hw, "IBM", "ProFibre 4000R", MULTIBUS, DEFAULT_GETUID);
	ADDHWE(hw, "SGI", "TP9100", MULTIBUS, DEFAULT_GETUID);
	ADDHWE(hw, "SGI", "TP9300", MULTIBUS, DEFAULT_GETUID);
	ADDHWE(hw, "SGI", "TP9400", MULTIBUS, DEFAULT_GETUID);
	ADDHWE(hw, "SGI", "TP9500", MULTIBUS, DEFAULT_GETUID);
	ADDHWE(hw, "3PARdata", "VV", MULTIBUS, DEFAULT_GETUID);
	ADDHWE(hw, "STK", "OPENstorage D280", GROUP_BY_SERIAL, DEFAULT_GETUID);
	ADDHWE(hw, "SUN", "StorEdge 3510", MULTIBUS, DEFAULT_GETUID);
	ADDHWE(hw, "SUN", "T4", MULTIBUS, DEFAULT_GETUID);

	ADDHWE_EXT(hw, "DGC", "*", GROUP_BY_PRIO, DEFAULT_GETUID,
		   DEFAULT_GETUID " -p 0xc0", "1 emc", "0", "emc_clariion");
	ADDHWE_EXT(hw, "IBM", "3542", GROUP_BY_SERIAL, DEFAULT_GETUID,
		   NULL, "0", "0", "tur");
}

