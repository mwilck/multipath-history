#include <stdio.h>

#include "memory.h"
#include "vector.h"
#include "util.h"
#include "debug.h"
#include "regex.h"
#include "blacklist.h"

void
setup_default_blist (vector blist)
{
	struct blentry * ble;

	VECTOR_ADDSTR(blist, "(ram|raw|loop|fd|md|dm-|sr|scd|st)[0-9]*", 40);
	VECTOR_ADDSTR(blist, "hd[a-z][[0-9]*]", 15);
	VECTOR_ADDSTR(blist, "cciss!c[0-9]d[0-9]*[p[0-9]*]", 28);
}

int
blacklist (vector blist, char * dev)
{
	int i;
	struct blentry *ble;

	vector_foreach_slot (blist, ble, i) {
		if (!regexec(ble->preg, dev, 0, NULL, 0)) {
			condlog(3, "%s blacklisted", dev);
			return 1;
		}
	}
	return 0;
}

void
store_regex (vector blist, char * regex)
{
	int len;
	struct blentry *ble;

	if (!blist)
		return;

	if (!regex)
		return;

	len = strlen(regex);

	VECTOR_ADDSTR(blist, regex, len);
}	
