#include <stdio.h>
#include <string.h>
#include <vector.h>
#include <checkers.h>
#include <hwtable.h>

#include "structs.h"
#include "pgpolicies.h"
#include "config.h"
#include "debug.h"

extern struct mpentry *
find_mpe (char * wwid)
{
	int i;
	struct mpentry * mpe;

	vector_foreach_slot (conf->mptable, mpe, i)
                if (mpe->wwid && strcmp(mpe->wwid, wwid) == 0)
			return mpe;

	return NULL;
}

/*
 * selectors :
 * traverse the configuration layers from most specific to most generic
 * stop at first explicit setting found
 */
extern int
select_pgpolicy (struct multipath * mp)
{
	struct path * pp;
	char pgpolicy_name[POLICY_NAME_SIZE];

	pp = VECTOR_SLOT(mp->paths, 0);

	if (conf->pgpolicy_flag > 0) {
		mp->pgpolicy = conf->pgpolicy_flag;
		get_pgpolicy_name(pgpolicy_name, mp->pgpolicy);
		dbg("pgpolicy = %s (cmd line flag)", pgpolicy_name);
		return 0;
	}
	if (mp->mpe && mp->mpe->pgpolicy > 0) {
		mp->pgpolicy = mp->mpe->pgpolicy;
		get_pgpolicy_name(pgpolicy_name, mp->pgpolicy);
		dbg("pgpolicy = %s (LUN setting)", pgpolicy_name);
		return 0;
	}
	if (mp->hwe && mp->hwe->pgpolicy > 0) {
		mp->pgpolicy = mp->hwe->pgpolicy;
		get_pgpolicy_name(pgpolicy_name, mp->pgpolicy);
		dbg("pgpolicy = %s (controler setting)", pgpolicy_name);
		return 0;
	}
	if (conf->default_pgpolicy > 0) {
		mp->pgpolicy = conf->default_pgpolicy;
		get_pgpolicy_name(pgpolicy_name, mp->pgpolicy);
		dbg("pgpolicy = %s (config file default)", pgpolicy_name);
		return 0;
	}
	mp->pgpolicy = FAILOVER;
	get_pgpolicy_name(pgpolicy_name, FAILOVER);
	dbg("pgpolicy = %s (internal default)", pgpolicy_name);
	return 0;
}

extern int
select_selector (struct multipath * mp)
{
	if (mp->mpe && mp->mpe->selector) {
		mp->selector = mp->mpe->selector;
		dbg("selector = %s (LUN setting)", mp->selector);
		return 0;
	}
	if (mp->hwe && mp->hwe->selector) {
		mp->selector = mp->hwe->selector;
		dbg("selector = %s (controler setting)", mp->selector);
		return 0;
	}
	mp->selector = conf->default_selector;
	dbg("selector = %s (internal default)", mp->selector);
	return 0;
}

extern int
select_alias (struct multipath * mp)
{
	if (mp->mpe && mp->mpe->alias)
		mp->alias = mp->mpe->alias;
	else
		mp->alias = mp->wwid;

	return 0;
}

extern int
select_features (struct multipath * mp)
{
	if (mp->hwe && mp->hwe->features) {
		mp->features = mp->hwe->features;
		dbg("features = %s (controler setting)", mp->features);
		return 0;
	}
	mp->features = conf->default_features;
	dbg("features = %s (internal default)", mp->features);
	return 0;
}

extern int
select_hwhandler (struct multipath * mp)
{
	if (mp->hwe && mp->hwe->hwhandler) {
		mp->hwhandler = mp->hwe->hwhandler;
		dbg("hwhandler = %s (controler setting)", mp->hwhandler);
		return 0;
	}
	mp->hwhandler = conf->default_hwhandler;
	dbg("hwhandler = %s (internal default)", mp->hwhandler);
	return 0;
}

extern int
select_checkfn(struct path *pp)
{
	char checker_name[CHECKER_NAME_SIZE];
	struct hwentry * hwe = NULL;

	hwe = find_hw(conf->hwtable, pp->vendor_id, pp->product_id);

	if (hwe && hwe->checker_index > 0) {
		get_checker_name(checker_name, hwe->checker_index);
		dbg("path checker = %s (controler setting)", checker_name);
		pp->checkfn = get_checker_addr(hwe->checker_index);
		return 0;
	}
	pp->checkfn = &readsector0;
	get_checker_name(checker_name, READSECTOR0);
	dbg("path checker = %s (internal default)", checker_name);
	return 0;
}

extern int
select_getuid (struct path * pp)
{
	struct hwentry * hwe = NULL;

	hwe = find_hw(conf->hwtable, pp->vendor_id, pp->product_id);

	if (hwe && hwe->getuid) {
		pp->getuid = hwe->getuid;
		dbg("getuid = %s (controler setting)", pp->getuid);
		return 0;
	}
	pp->getuid = conf->default_getuid;
	dbg("getuid = %s (internal default)", pp->getuid);
	return 0;
}

extern int
select_getprio (struct path * pp)
{
	struct hwentry * hwe = NULL;

	hwe = find_hw(conf->hwtable, pp->vendor_id, pp->product_id);

	if (hwe && hwe->getprio) {
		pp->getprio = hwe->getprio;
		dbg("getprio = %s (controler setting)", pp->getprio);
		return 0;
	}
	pp->getprio = conf->default_getprio;
	dbg("getprio = %s (internal default)", pp->getprio);
	return 0;
}

