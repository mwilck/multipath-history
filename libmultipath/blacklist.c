#include <stdio.h>

#include "memory.h"
#include "vector.h"
#include "util.h"
#include "debug.h"
#include "regex.h"
#include "blacklist.h"

static int
store_ble (vector blist, char * str, int len)
{
	struct blentry * ble;
	
	ble = zalloc(sizeof(struct blentry));

	if (!ble)
		return 1;

	ble->str = zalloc(len * sizeof(char));

	if (!ble->str) 
		goto out;

	ble->preg = zalloc(sizeof(regex_t));

	if (!ble->preg)
		goto out1;

	snprintf(ble->str, len, str);

	if (regcomp((regex_t *)ble->preg, ble->str, REG_EXTENDED|REG_NOSUB))
		goto out2;

	if (!vector_alloc_slot(blist))
		goto out2;

	vector_set_slot(blist, ble);
	return 0;
out2:
	free(ble->preg);
out1:
	free(ble->str);
out:
	free(ble);
	return 1;
}

int
setup_default_blist (vector blist)
{
	int r = 0;

	r += store_ble(blist, "(ram|raw|loop|fd|md|dm-|sr|scd|st)[0-9]*", 40);
	r += store_ble(blist, "hd[a-z][[0-9]*]", 15);
	r += store_ble(blist, "cciss!c[0-9]d[0-9]*[p[0-9]*]", 28);

	return r;
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

int
store_regex (vector blist, char * regex)
{
	int len;

	if (!blist)
		return 1;

	if (!regex)
		return 1;

	len = strlen(regex);

	return store_ble(blist, regex, len);
}	
