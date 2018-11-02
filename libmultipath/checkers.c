#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <dlfcn.h>
#include <sys/stat.h>

#include "debug.h"
#include "checkers.h"
#include "vector.h"

char *checker_state_names[] = {
	"wild",
	"unchecked",
	"down",
	"up",
	"shaky",
	"ghost",
	"pending",
	"timeout",
	"removed",
	"delayed",
};

static LIST_HEAD(checkers);

char * checker_state_name (int i)
{
	return checker_state_names[i];
}

int init_checkers (char *multipath_dir)
{
	if (!add_checker(multipath_dir, DEFAULT_CHECKER))
		return 1;
	return 0;
}

struct checker * alloc_checker (void)
{
	struct checker *c;

	c = MALLOC(sizeof(struct checker));
	if (c) {
		INIT_LIST_HEAD(&c->node);
		c->refcount = 1;
		c->fd = -1;
	}
	return c;
}

void free_checker (struct checker * c)
{
	if (!c)
		return;
	c->refcount--;
	if (c->refcount) {
		condlog(3, "%s checker refcount %d",
			c->name, c->refcount);
		return;
	}
	condlog(3, "unloading %s checker", c->name);
	list_del(&c->node);
	if (c->handle) {
		if (dlclose(c->handle) != 0) {
			condlog(0, "Cannot unload checker %s: %s",
				c->name, dlerror());
		}
	}
	FREE(c);
}

void cleanup_checkers (void)
{
	struct checker * checker_loop;
	struct checker * checker_temp;

	list_for_each_entry_safe(checker_loop, checker_temp, &checkers, node) {
		free_checker(checker_loop);
	}
}

struct checker * checker_lookup (char * name)
{
	struct checker * c;

	if (!name || !strlen(name))
		return NULL;
	list_for_each_entry(c, &checkers, node) {
		if (!strncmp(name, c->name, CHECKER_NAME_LEN))
			return c;
	}
	return NULL;
}

struct checker * add_checker (char *multipath_dir, char * name)
{
	char libname[LIB_CHECKER_NAMELEN];
	struct stat stbuf;
	struct checker * c;
	char *errstr;

	c = alloc_checker();
	if (!c)
		return NULL;
	snprintf(c->name, CHECKER_NAME_LEN, "%s", name);
	if (!strncmp(c->name, NONE, 4))
		goto done;
	snprintf(libname, LIB_CHECKER_NAMELEN, "%s/libcheck%s.so",
		 multipath_dir, name);
	if (stat(libname,&stbuf) < 0) {
		condlog(0,"Checker '%s' not found in %s",
			name, multipath_dir);
		goto out;
	}
	condlog(3, "loading %s checker", libname);
	c->handle = dlopen(libname, RTLD_NOW);
	if (!c->handle) {
		if ((errstr = dlerror()) != NULL)
			condlog(0, "A dynamic linking error occurred: (%s)",
				errstr);
		goto out;
	}
	c->check = (int (*)(struct checker *)) dlsym(c->handle, "libcheck_check");
	errstr = dlerror();
	if (errstr != NULL)
		condlog(0, "A dynamic linking error occurred: (%s)", errstr);
	if (!c->check)
		goto out;

	c->init = (int (*)(struct checker *)) dlsym(c->handle, "libcheck_init");
	errstr = dlerror();
	if (errstr != NULL)
		condlog(0, "A dynamic linking error occurred: (%s)", errstr);
	if (!c->init)
		goto out;

	c->free = (void (*)(struct checker *)) dlsym(c->handle, "libcheck_free");
	errstr = dlerror();
	if (errstr != NULL)
		condlog(0, "A dynamic linking error occurred: (%s)", errstr);
	if (!c->free)
		goto out;

	c->msgtable_size = 0;
	c->msgtable = dlsym(c->handle, "libcheck_msgtable");

	if (c->msgtable != NULL) {
		const char **p;

		for (p = c->msgtable;
		     *p && (p - c->msgtable) < CHECKER_MSGTABLE_SIZE; p++)
			/* nothing */;

		c->msgtable_size = p - c->msgtable;
	} else
		c->msgtable_size = 0;
	condlog(3, "checker %s: message table size = %d",
		c->name, c->msgtable_size);

done:
	c->fd = -1;
	c->sync = 1;
	list_add(&c->node, &checkers);
	return c;
out:
	free_checker(c);
	return NULL;
}

void checker_set_fd (struct checker * c, int fd)
{
	if (!c)
		return;
	c->fd = fd;
}

void checker_set_sync (struct checker * c)
{
	if (!c)
		return;
	c->sync = 1;
}

void checker_set_async (struct checker * c)
{
	if (!c)
		return;
	c->sync = 0;
}

void checker_enable (struct checker * c)
{
	if (!c)
		return;
	c->disable = 0;
}

void checker_disable (struct checker * c)
{
	if (!c)
		return;
	c->disable = 1;
}

int checker_init (struct checker * c, void ** mpctxt_addr)
{
	if (!c)
		return 1;
	c->mpcontext = mpctxt_addr;
	if (c->init)
		return c->init(c);
	return 0;
}

void checker_clear (struct checker *c)
{
	memset(c, 0x0, sizeof(struct checker));
	c->fd = -1;
}

void checker_put (struct checker * dst)
{
	struct checker * src;

	if (!dst || !strlen(dst->name))
		return;
	src = checker_lookup(dst->name);
	if (dst->free)
		dst->free(dst);
	checker_clear(dst);
	free_checker(src);
}

int checker_check (struct checker * c, int path_state)
{
	int r;

	if (!c)
		return PATH_WILD;

	c->msgid = CHECKER_MSGID_NONE;
	if (c->disable) {
		c->msgid = CHECKER_MSGID_DISABLED;
		return PATH_UNCHECKED;
	}
	if (!strncmp(c->name, NONE, 4))
		return path_state;

	if (c->fd < 0) {
		c->msgid = CHECKER_MSGID_NO_FD;
		return PATH_WILD;
	}
	r = c->check(c);

	return r;
}

int checker_selected (struct checker * c)
{
	if (!c)
		return 0;
	if (!strncmp(c->name, NONE, 4))
		return 1;
	return (c->check) ? 1 : 0;
}

const char *checker_name(const struct checker *c)
{
	if (!c)
		return NULL;
	return c->name;
}

static const char *generic_msg[CHECKER_GENERIC_MSGTABLE_SIZE] = {
	[CHECKER_MSGID_NONE] = "",
	[CHECKER_MSGID_DISABLED] = " is disabled",
	[CHECKER_MSGID_NO_FD] = " has no usable fd",
	[CHECKER_MSGID_INVALID] = " provided invalid message id",
	[CHECKER_MSGID_UP] = " reports path is up",
	[CHECKER_MSGID_DOWN] = " reports path is down",
	[CHECKER_MSGID_GHOST] = " reports path is ghost",
	[CHECKER_MSGID_UNSUPPORTED] = " doesn't support this device",
};

const char *checker_message(const struct checker *c)
{
	int id;

	if (!c || c->msgid < 0 ||
	    (c->msgid >= CHECKER_GENERIC_MSGTABLE_SIZE &&
	     c->msgid < CHECKER_FIRST_MSGID))
		goto bad_id;

	if (c->msgid < CHECKER_GENERIC_MSGTABLE_SIZE)
		return generic_msg[c->msgid];

	id = c->msgid - CHECKER_FIRST_MSGID;
	if (id < c->cls->msgtable_size)
		return c->cls->msgtable[id];

bad_id:
	return generic_msg[CHECKER_MSGID_NONE];
}

void checker_clear_message (struct checker *c)
{
	if (!c)
		return;
	c->msgid = CHECKER_MSGID_NONE;
}

void checker_get (char *multipath_dir, struct checker * dst, char * name)
{
	struct checker * src = NULL;

	if (!dst)
		return;

	if (name && strlen(name)) {
		src = checker_lookup(name);
		if (!src)
			src = add_checker(multipath_dir, name);
	}
	if (!src) {
		dst->check = NULL;
		return;
	}
	dst->fd = src->fd;
	dst->sync = src->sync;
	strncpy(dst->name, src->name, CHECKER_NAME_LEN);
	dst->msgid = CHECKER_MSGID_NONE;
	dst->check = src->check;
	dst->init = src->init;
	dst->free = src->free;
	dst->msgtable = src->msgtable;
	dst->msgtable_size = src->msgtable_size;
	dst->handle = NULL;
	src->refcount++;
}
