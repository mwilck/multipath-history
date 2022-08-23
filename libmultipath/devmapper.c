#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libdevmapper.h>
#include <ctype.h>
#include <linux/kdev_t.h>

#include "vector.h"
#include "structs.h"
#include "debug.h"
#include "memory.h"

extern int
dm_prereq (char * str, int x, int y, int z)
{
	int r = 1;
	struct dm_task *dmt;
	struct dm_versions *target;
	struct dm_versions *last_target;

	if (!(dmt = dm_task_create(DM_DEVICE_LIST_VERSIONS)))
		return 1;

	dm_task_no_open_count(dmt);

	if (!dm_task_run(dmt))
		goto out;

	target = dm_task_get_versions(dmt);

	/* Fetch targets and print 'em */
	do {
		last_target = target;

		if (!strncmp(str, target->name, strlen(str)) &&
		    /* dummy prereq on multipath version */
		    target->version[0] >= x &&
		    target->version[1] >= y &&
		    target->version[2] >= z
		   )
			r = 0;

		target = (void *) target + target->next;
	} while (last_target != target);

	out:
	dm_task_destroy(dmt);
	return r;
}

extern int
dm_simplecmd (int task, const char *name) {
	int r = 0;
	struct dm_task *dmt;

	if (!(dmt = dm_task_create (task)))
		return 0;

	if (!dm_task_set_name (dmt, name))
		goto out;

	dm_task_no_open_count(dmt);

	r = dm_task_run (dmt);

	out:
	dm_task_destroy (dmt);
	return r;
}

extern int
dm_addmap (int task, const char *name, const char *target,
	   const char *params, unsigned long size) {
	int r = 0;
	struct dm_task *dmt;

	if (!(dmt = dm_task_create (task)))
		return 0;

	if (!dm_task_set_name (dmt, name))
		goto addout;

	if (!dm_task_add_target (dmt, 0, size, target, params))
		goto addout;

	dm_task_no_open_count(dmt);

	r = dm_task_run (dmt);

	addout:
	dm_task_destroy (dmt);
	return r;
}

extern int
dm_map_present (char * str)
{
	int r = 0;
	struct dm_task *dmt;
	struct dm_info info;

	if (!(dmt = dm_task_create(DM_DEVICE_INFO)))
		return 0;

	if (!dm_task_set_name(dmt, str))
		goto out;

	dm_task_no_open_count(dmt);

	if (!dm_task_run(dmt))
		goto out;

	if (!dm_task_get_info(dmt, &info))
		goto out;

	if (info.exists)
		r = 1;
out:
	dm_task_destroy(dmt);
	return r;
}

extern int
dm_get_map(char * name, unsigned long * size, char * outparams)
{
	int r = 1;
	struct dm_task *dmt;
	void *next = NULL;
	uint64_t start, length;
	char *target_type = NULL;
	char *params = NULL;

	if (!(dmt = dm_task_create(DM_DEVICE_TABLE)))
		return 1;

	if (!dm_task_set_name(dmt, name))
		goto out;

	dm_task_no_open_count(dmt);

	if (!dm_task_run(dmt))
		goto out;

	/* Fetch 1st target */
	next = dm_get_next_target(dmt, next, &start, &length,
				  &target_type, &params);

	if (size)
		*size = length;

	if (snprintf(outparams, PARAMS_SIZE, "%s", params) <= PARAMS_SIZE)
		r = 0;
out:
	dm_task_destroy(dmt);
	return r;
}

extern int
dm_get_status(char * name, char * outstatus)
{
	int r = 1;
	struct dm_task *dmt;
	void *next = NULL;
	uint64_t start, length;
	char *target_type;
	char *status;

	if (!(dmt = dm_task_create(DM_DEVICE_STATUS)))
		return 1;

	if (!dm_task_set_name(dmt, name))
		goto out;

	dm_task_no_open_count(dmt);

	if (!dm_task_run(dmt))
		goto out;

	/* Fetch 1st target */
	next = dm_get_next_target(dmt, next, &start, &length,
				  &target_type, &status);

	if (snprintf(outstatus, PARAMS_SIZE, "%s", status) <= PARAMS_SIZE)
		r = 0;
out:
	dm_task_destroy(dmt);
	return r;
}

extern int
dm_type(char * name, char * type)
{
	int r = 0;
	struct dm_task *dmt;
	void *next = NULL;
	uint64_t start, length;
	char *target_type = NULL;
	char *params;

	if (!(dmt = dm_task_create(DM_DEVICE_TABLE)))
		return 0;

	if (!dm_task_set_name(dmt, name))
		goto out;

	dm_task_no_open_count(dmt);

	if (!dm_task_run(dmt))
		goto out;

	/* Fetch 1st target */
	next = dm_get_next_target(dmt, next, &start, &length,
				  &target_type, &params);

	if (0 == strcmp(target_type, type))
		r = 1;

out:
	dm_task_destroy(dmt);
	return r;
}

int
dm_get_opencount (char * mapname)
{
	int r = -1;
	struct dm_task *dmt;
	struct dm_info info;

	if (!(dmt = dm_task_create(DM_DEVICE_INFO)))
		return 0;

	if (!dm_task_set_name(dmt, mapname))
		goto out;

	dm_task_no_open_count(dmt);

	if (!dm_task_run(dmt))
		goto out;

	if (!dm_task_get_info(dmt, &info))
		goto out;

	r = info.open_count;
out:
	dm_task_destroy(dmt);
	return r;
}
	
extern int
dm_flush_maps (char * type)
{
	int r = 0;
	struct dm_task *dmt;
	struct dm_names *names;
	unsigned next = 0;

	if (!(dmt = dm_task_create (DM_DEVICE_LIST)))
		return 0;

	dm_task_no_open_count(dmt);

	if (!dm_task_run (dmt))
		goto out;

	if (!(names = dm_task_get_names (dmt)))
		goto out;

	if (!names->dev)
		goto out;

	do {
		if (dm_type(names->name, type) &&
		    dm_get_opencount(names->name) == 0 &&
		    !dm_simplecmd(DM_DEVICE_REMOVE, names->name))
			r++;

		next = names->next;
		names = (void *) names + next;
	} while (next);

	out:
	dm_task_destroy (dmt);
	return r;
}

int
dm_fail_path(char * mapname, char * path)
{
	int r = 0;
	int sz;
	struct dm_task *dmt;
	char *str;

	if (!(dmt = dm_task_create(DM_DEVICE_TARGET_MSG)))
		return 0;

	if (!dm_task_set_name(dmt, mapname))
		goto out;

	if (!dm_task_set_sector(dmt, 0))
		goto out;

	sz = strlen(path) + 11;
	str = malloc(sz);

	snprintf(str, sz, "fail_path %s\n", path);

	if (!dm_task_set_message(dmt, str))
		goto out;

	free(str);

	dm_task_no_open_count(dmt);

	if (!dm_task_run(dmt))
		goto out;

	r = 1;

	out:
	dm_task_destroy(dmt);

	return r;
}

int
dm_reinstate(char * mapname, char * path)
{
	int r = 0;
	int sz;
	struct dm_task *dmt;
	char *str;

	if (!(dmt = dm_task_create(DM_DEVICE_TARGET_MSG)))
		return 0;

	if (!dm_task_set_name(dmt, mapname))
		goto out;

	if (!dm_task_set_sector(dmt, 0))
		goto out;

	sz = strlen(path) + 16;
	str = malloc(sz);

	snprintf(str, sz, "reinstate_path %s\n", path);

	if (!dm_task_set_message(dmt, str))
		goto out;

	free(str);

	dm_task_no_open_count(dmt);

	if (!dm_task_run(dmt))
		goto out;

	r = 1;

	out:
	dm_task_destroy(dmt);

	return r;
}

int
dm_switchgroup(char * mapname, int index)
{
	int r = 0;
	struct dm_task *dmt;
	char str[24];

	if (!(dmt = dm_task_create(DM_DEVICE_TARGET_MSG)))
		return 0;

	if (!dm_task_set_name(dmt, mapname))
		goto out;

	if (!dm_task_set_sector(dmt, 0))
		goto out;

	snprintf(str, 24, "switch_group %i\n", index);
	condlog(3, "message %s 0 %s", mapname, str);

	if (!dm_task_set_message(dmt, str))
		goto out;

	dm_task_no_open_count(dmt);

	if (!dm_task_run(dmt))
		goto out;

	r = 1;

	out:
	dm_task_destroy(dmt);

	return r;
}

int
dm_get_maps (vector mp, char * type)
{
	struct multipath * mpp;
	int r = 0;
	struct dm_task *dmt;
	struct dm_names *names;
	unsigned next = 0;
	unsigned long length;
	char *params;
	char *status;

	if (!type)
		return 0;

	if (!(dmt = dm_task_create (DM_DEVICE_LIST)))
		return 0;

	dm_task_no_open_count(dmt);

	if (!dm_task_run (dmt))
		goto out;

	if (!(names = dm_task_get_names (dmt)))
		goto out;

	if (!names->dev)
		goto out;

	do {
		if (dm_type(names->name, type)) {
			mpp = (struct multipath *)
				MALLOC(sizeof(struct multipath));

			if (!mpp) {
				r = 1;
				goto out;

			if (dm_get_map(names->name, &mpp->size, mpp->params))
				goto out1;

			if (dm_get_status(names->name, mpp->status))
				goto out1;

			mpp->alias = MALLOC(strlen(names->name) + 1);

			if (!mpp->alias)
				goto out1;

			strncat(mpp->alias, names->name, strlen(names->name));

			if (!vector_alloc_slot(mp))
				goto out1;
			
			vector_set_slot(mp, mpp);
			mpp = NULL;
		}
                next = names->next;
                names = (void *) names + next;
	} while (next);

	out:
	dm_task_destroy (dmt);
	return r;
}

int
dm_geteventnr (char *name)
{
	struct dm_task *dmt;
	struct dm_info info;

	if (!(dmt = dm_task_create(DM_DEVICE_INFO)))
		return 0;

	if (!dm_task_set_name(dmt, name))
		goto out;

	dm_task_no_open_count(dmt);

	if (!dm_task_run(dmt))
		goto out;

	if (!dm_task_get_info(dmt, &info)) {
		info.event_nr = 0;
		goto out;
	}

	if (!info.exists) {
		info.event_nr = 0;
		goto out;
	}

out:
	dm_task_destroy(dmt);

	return info.event_nr;
}
