#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libdevmapper.h>
#include <ctype.h>
#include <linux/kdev_t.h>

extern int
dm_prereq (char * str, int x, int y, int z)
{
	int r = 1;
	struct dm_task *dmt;
	struct dm_versions *target;
	struct dm_versions *last_target;

	if (!(dmt = dm_task_create(DM_DEVICE_LIST_VERSIONS)))
		return 1;

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
	struct dm_names *names;
	unsigned next = 0;

	if (!(dmt = dm_task_create (DM_DEVICE_LIST)))
		return 0;

	if (!dm_task_run (dmt))
		goto out;

	if (!(names = dm_task_get_names (dmt)))
		goto out;

	if (!names->dev)
		goto out;

	do {
		if (0 == strcmp (names->name, str))
			r = 1;

		next = names->next;
		names = (void *) names + next;
	} while (next);

	out:
	dm_task_destroy (dmt);
	return r;
}

extern int
dm_get_map(char * name, unsigned long * size, char ** outparams)
{
	int r = 0;
	struct dm_task *dmt;
	void *next = NULL;
	uint64_t start, length;
	char *target_type = NULL;
	char *params;
	int cmd;

	cmd = DM_DEVICE_TABLE;

	if (!(dmt = dm_task_create(cmd)))
		return 0;

	if (!dm_task_set_name(dmt, name))
		goto out;

	if (!dm_task_run(dmt))
		goto out;

	/* Fetch 1st target */
	next = dm_get_next_target(dmt, next, &start, &length,
				  &target_type, &params);

	if (size)
		*size = length;

	*outparams = malloc(strlen(params) + 1);

	if (!*outparams)
		goto out;

	if (sprintf(*outparams, params))
		goto out;

	r = 1;

	out:
	dm_task_destroy(dmt);
	return r;
}

extern int
dm_get_status(char * name, char ** outstatus)
{
	int r = 0;
	struct dm_task *dmt;
	void *next = NULL;
	uint64_t start, length;
	char *target_type = NULL;
	char *status;
	int cmd;

	cmd = DM_DEVICE_STATUS;

	if (!(dmt = dm_task_create(cmd)))
		return 0;

	if (!dm_task_set_name(dmt, name))
		goto out;

	if (!dm_task_run(dmt))
		goto out;

	/* Fetch 1st target */
	next = dm_get_next_target(dmt, next, &start, &length,
				  &target_type, &status);

	*outstatus = malloc(strlen(status) + 1);

	if (!*outstatus)
		goto out;

	if (sprintf(*outstatus, status))
		goto out;

	r = 1;

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
	int cmd;

	cmd = DM_DEVICE_TABLE;

	if (!(dmt = dm_task_create(cmd)))
		return 0;

	if (!dm_task_set_name(dmt, name))
		goto out;

	if (!dm_task_run(dmt))
		goto out;

	/* Fetch 1st target */
	next = dm_get_next_target(dmt, next, &start, &length,
				  &target_type, &params);

	if (0 == strcmp(target_type, type)) {
		r = 1;
		goto out;
	}

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

	if (!dm_task_run (dmt))
		goto out;

	if (!(names = dm_task_get_names (dmt)))
		goto out;

	if (!names->dev)
		goto out;

	do {
		if (dm_type(names->name, type) &&
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

	if (!dm_task_run(dmt))
		goto out;

	r = 1;

	out:
	dm_task_destroy(dmt);

	return r;
}

