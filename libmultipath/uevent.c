/*
 * uevent.c - trigger upon netlink uevents from the kernel
 *
 *	Only kernels from version 2.6.10* on provide the uevent netlink socket.
 *	Until the libc-kernel-headers are updated, you need to compile with:
 *
 *	  gcc -I /lib/modules/`uname -r`/build/include -o uevent_listen uevent_listen.c
 *
 * Copyright (C) 2004 Kay Sievers <kay.sievers@vrfy.org>
 *
 *	This program is free software; you can redistribute it and/or modify it
 *	under the terms of the GNU General Public License as published by the
 *	Free Software Foundation version 2 of the License.
 *
 *	This program is distributed in the hope that it will be useful, but
 *	WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *	General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License along
 *	with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/user.h>
#include <sys/un.h>
#include <poll.h>
#include <linux/types.h>
#include <linux/netlink.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <libudev.h>
#include <errno.h>

#include "memory.h"
#include "debug.h"
#include "list.h"
#include "uevent.h"
#include "vector.h"
#include "structs.h"
#include "util.h"
#include "config.h"
#include "blacklist.h"

#define MAX_ACCUMULATION_COUNT 2048
#define MAX_ACCUMULATION_TIME 30*1000
#define MIN_BURST_SPEED 10

typedef int (uev_trigger)(struct uevent *, void * trigger_data);

LIST_HEAD(uevq);
pthread_mutex_t uevq_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t *uevq_lockp = &uevq_lock;
pthread_cond_t uev_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t *uev_condp = &uev_cond;
uev_trigger *my_uev_trigger;
void * my_trigger_data;
int servicing_uev;

int is_uevent_busy(void)
{
	int empty;

	pthread_mutex_lock(uevq_lockp);
	empty = list_empty(&uevq);
	pthread_mutex_unlock(uevq_lockp);
	return (!empty || servicing_uev);
}

struct uevent * alloc_uevent (void)
{
	struct uevent *uev = MALLOC(sizeof(struct uevent));

	if (uev) {
		INIT_LIST_HEAD(&uev->node);
		INIT_LIST_HEAD(&uev->merge_node);
	}

	return uev;
}

void
uevq_cleanup(struct list_head *tmpq)
{
	struct uevent *uev, *tmp;

	list_for_each_entry_safe(uev, tmp, tmpq, node) {
		list_del_init(&uev->node);

		if (uev->udev)
			udev_device_unref(uev->udev);
		FREE(uev);
	}
}

void
uevent_get_wwid(struct uevent *uev)
{
	int i;
	char *uid_attribute;
	struct config * conf;

	conf = get_multipath_config();
	uid_attribute = parse_uid_attribute_by_attrs(conf->uid_attrs, uev->kernel);
	put_multipath_config(conf);

	if (!uid_attribute)
		return;

	for (i = 0; uev->envp[i] != NULL; i++) {
		if (!strncmp(uev->envp[i], uid_attribute, strlen(uid_attribute)) &&
		    strlen(uev->envp[i]) > strlen(uid_attribute) &&
		    uev->envp[i][strlen(uid_attribute)] == '=') {
			uev->wwid = uev->envp[i] + strlen(uid_attribute) + 1;
			break;
		}
	}
	free(uid_attribute);
}

bool
uevent_need_merge(void)
{
	struct config * conf;
	bool need_merge = false;

	conf = get_multipath_config();
	if (conf->uid_attrs)
		need_merge = true;
	put_multipath_config(conf);

	return need_merge;
}

static bool
uevent_can_discard_by_devpath(const char *devpath)
{
	static const char BLOCK[] = "/block/";
	const char *tmp = strstr(devpath, BLOCK);

	if (tmp == NULL) {
		condlog(4, "no /block/ in '%s'", devpath);
		return true;
	}
	tmp += sizeof(BLOCK) - 1;
	if (*tmp == '\0')
		/* just ".../block/" - discard */
		return true;
	/*
	 * If there are more path elements after ".../block/xyz",
	 * it's a partition - discard it; but don't discard ".../block/sda/".
	 */
	tmp = strchr(tmp, '/');
	return tmp != NULL && *(tmp + 1) != '\0';
}

bool
uevent_can_discard(struct uevent *uev)
{
	struct config * conf;

	if (uevent_can_discard_by_devpath(uev->devpath))
		return true;

	/*
	 * do not filter dm devices by devnode
	 */
	if (!strncmp(uev->kernel, "dm-", 3))
		return false;
	/*
	 * filter paths devices by devnode
	 */
	conf = get_multipath_config();
	if (filter_devnode(conf->blist_devnode, conf->elist_devnode,
			   uev->kernel) > 0) {
		put_multipath_config(conf);
		return true;
	}
	put_multipath_config(conf);

	return false;
}

bool
uevent_can_filter(struct uevent *earlier, struct uevent *later)
{

	/*
	 * filter earlier uvents if path has removed later. Eg:
	 * "add path1 |chang path1 |add path2 |remove path1"
	 * can filter as:
	 * "add path2 |remove path1"
	 * uevents "add path1" and "chang path1" are filtered out
	 */
	if (!strcmp(earlier->kernel, later->kernel) &&
		!strcmp(later->action, "remove") &&
		strncmp(later->kernel, "dm-", 3)) {
		return true;
	}

	/*
	 * filter change uvents if add uevents exist. Eg:
	 * "change path1| add path1 |add path2"
	 * can filter as:
	 * "add path1 |add path2"
	 * uevent "chang path1" is filtered out
	 */
	if (!strcmp(earlier->kernel, later->kernel) &&
		!strcmp(earlier->action, "change") &&
		!strcmp(later->action, "add") &&
		strncmp(later->kernel, "dm-", 3)) {
		return true;
	}

	return false;
}

bool
merge_need_stop(struct uevent *earlier, struct uevent *later)
{
	/*
	 * dm uevent do not try to merge with left uevents
	 */
	if (!strncmp(later->kernel, "dm-", 3))
		return true;

	/*
	 * we can not make a jugement without wwid,
	 * so it is sensible to stop merging
	 */
	if (!earlier->wwid || !later->wwid)
		return true;
	/*
	 * uevents merging stoped
	 * when we meet an opposite action uevent from the same LUN to AVOID
	 * "add path1 |remove path1 |add path2 |remove path2 |add path3"
	 * to merge as "remove path1, path2" and "add path1, path2, path3"
	 * OR
	 * "remove path1 |add path1 |remove path2 |add path2 |remove path3"
	 * to merge as "add path1, path2" and "remove path1, path2, path3"
	 * SO
	 * when we meet a non-change uevent from the same LUN
	 * with the same wwid and different action
	 * it would be better to stop merging.
	 */
	if (!strcmp(earlier->wwid, later->wwid) &&
	    strcmp(earlier->action, later->action) &&
	    strcmp(earlier->action, "change") &&
	    strcmp(later->action, "change"))
		return true;

	return false;
}

bool
uevent_can_merge(struct uevent *earlier, struct uevent *later)
{
	/* merge paths uevents
	 * whose wwids exsit and are same
	 * and actions are same,
	 * and actions are addition or deletion
	 */
	if (earlier->wwid && later->wwid &&
	    !strcmp(earlier->wwid, later->wwid) &&
	    !strcmp(earlier->action, later->action) &&
	    strncmp(earlier->action, "change", 6) &&
	    strncmp(earlier->kernel, "dm-", 3)) {
		return true;
	}

	return false;
}

void
uevent_prepare(struct list_head *tmpq)
{
	struct uevent *uev, *tmp;

	list_for_each_entry_reverse_safe(uev, tmp, tmpq, node) {
		if (uevent_can_discard(uev)) {
			list_del_init(&uev->node);
			if (uev->udev)
				udev_device_unref(uev->udev);
			FREE(uev);
			continue;
		}

		if (strncmp(uev->kernel, "dm-", 3) &&
		    uevent_need_merge())
			uevent_get_wwid(uev);
	}
}

void
uevent_filter(struct uevent *later, struct list_head *tmpq)
{
	struct uevent *earlier, *tmp;

	list_for_some_entry_reverse_safe(earlier, tmp, &later->node, tmpq, node) {
		/*
		 * filter unnessary earlier uevents
		 * by the later uevent
		 */
		if (uevent_can_filter(earlier, later)) {
			condlog(2, "uevent: %s-%s has filtered by uevent: %s-%s",
				earlier->kernel, earlier->action,
				later->kernel, later->action);

			list_del_init(&earlier->node);
			if (earlier->udev)
				udev_device_unref(earlier->udev);
			FREE(earlier);
		}
	}
}

void
uevent_merge(struct uevent *later, struct list_head *tmpq)
{
	struct uevent *earlier, *tmp;

	list_for_some_entry_reverse_safe(earlier, tmp, &later->node, tmpq, node) {
		if (merge_need_stop(earlier, later))
			break;
		/*
		 * merge earlier uevents to the later uevent
		 */
		if (uevent_can_merge(earlier, later)) {
			condlog(2, "merged uevent: %s-%s-%s with uevent: %s-%s-%s",
				earlier->action, earlier->kernel, earlier->wwid,
				later->action, later->kernel, later->wwid);

			list_move(&earlier->node, &later->merge_node);
		}
	}
}

void
merge_uevq(struct list_head *tmpq)
{
	struct uevent *later;

	uevent_prepare(tmpq);
	list_for_each_entry_reverse(later, tmpq, node) {
		uevent_filter(later, tmpq);
		if(uevent_need_merge())
			uevent_merge(later, tmpq);
	}
}

void
service_uevq(struct list_head *tmpq)
{
	struct uevent *uev, *tmp;

	list_for_each_entry_safe(uev, tmp, tmpq, node) {
		list_del_init(&uev->node);

		if (my_uev_trigger && my_uev_trigger(uev, my_trigger_data))
			condlog(0, "uevent trigger error");

		uevq_cleanup(&uev->merge_node);

		if (uev->udev)
			udev_device_unref(uev->udev);
		FREE(uev);
	}
}

static void uevent_cleanup(void *arg)
{
	struct udev *udev = arg;

	condlog(3, "Releasing uevent_listen() resources");
	udev_unref(udev);
}

/*
 * Service the uevent queue.
 */
int uevent_dispatch(int (*uev_trigger)(struct uevent *, void * trigger_data),
		    void * trigger_data)
{
	my_uev_trigger = uev_trigger;
	my_trigger_data = trigger_data;

	mlockall(MCL_CURRENT | MCL_FUTURE);

	while (1) {
		LIST_HEAD(uevq_tmp);

		pthread_mutex_lock(uevq_lockp);
		servicing_uev = 0;
		/*
		 * Condition signals are unreliable,
		 * so make sure we only wait if we have to.
		 */
		if (list_empty(&uevq)) {
			pthread_cond_wait(uev_condp, uevq_lockp);
		}
		servicing_uev = 1;
		list_splice_init(&uevq, &uevq_tmp);
		pthread_mutex_unlock(uevq_lockp);
		if (!my_uev_trigger)
			break;
		merge_uevq(&uevq_tmp);
		service_uevq(&uevq_tmp);
	}
	condlog(3, "Terminating uev service queue");
	uevq_cleanup(&uevq);
	return 0;
}

struct uevent *uevent_from_buffer(char *buf, ssize_t buflen)
{
	struct uevent *uev;
	char *buffer;
	size_t bufpos;
	int i;
	char *pos;

	uev = alloc_uevent();
	if (!uev) {
		condlog(1, "lost uevent, oom");
		return NULL;
	}

	if ((size_t)buflen > sizeof(buf)-1)
		buflen = sizeof(buf)-1;

	/*
	 * Copy the shared receive buffer contents to buffer private
	 * to this uevent so we can immediately reuse the shared buffer.
	 */
	memcpy(uev->buffer, buf, HOTPLUG_BUFFER_SIZE + OBJECT_SIZE);
	buffer = uev->buffer;
	buffer[buflen] = '\0';

	/* save start of payload */
	bufpos = strlen(buffer) + 1;

	/* action string */
	uev->action = buffer;
	pos = strchr(buffer, '@');
	if (!pos) {
		condlog(3, "bad action string '%s'", buffer);
		FREE(uev);
		return NULL;
	}
	pos[0] = '\0';

	/* sysfs path */
	uev->devpath = &pos[1];

	/* hotplug events have the environment attached - reconstruct envp[] */
	for (i = 0; (bufpos < (size_t)buflen) && (i < HOTPLUG_NUM_ENVP-1); i++) {
		int keylen;
		char *key;

		key = &buffer[bufpos];
		keylen = strlen(key);
		uev->envp[i] = key;
		/* Filter out sequence number */
		if (strncmp(key, "SEQNUM=", 7) == 0) {
			char *eptr;

			uev->seqnum = strtoul(key + 7, &eptr, 10);
			if (eptr == key + 7)
				uev->seqnum = -1;
		}
		bufpos += keylen + 1;
	}
	uev->envp[i] = NULL;

	condlog(3, "uevent %ld '%s' from '%s'", uev->seqnum,
		uev->action, uev->devpath);
	uev->kernel = strrchr(uev->devpath, '/');
	if (uev->kernel)
		uev->kernel++;

	/* print payload environment */
	for (i = 0; uev->envp[i] != NULL; i++)
		condlog(5, "%s", uev->envp[i]);

	return uev;
}

int failback_listen(void)
{
	int sock;
	struct sockaddr_nl snl;
	struct sockaddr_un sun;
	socklen_t addrlen;
	int retval;
	int rcvbufsz = 128*1024;
	int rcvsz = 0;
	int rcvszsz = sizeof(rcvsz);
	unsigned int *prcvszsz = (unsigned int *)&rcvszsz;
	const int feature_on = 1;
	/*
	 * First check whether we have a udev socket
	 */
	memset(&sun, 0x00, sizeof(struct sockaddr_un));
	sun.sun_family = AF_LOCAL;
	strcpy(&sun.sun_path[1], "/org/kernel/dm/multipath_event");
	addrlen = offsetof(struct sockaddr_un, sun_path) + strlen(sun.sun_path+1) + 1;

	sock = socket(AF_LOCAL, SOCK_DGRAM, 0);
	if (sock >= 0) {

		condlog(3, "reading events from udev socket.");

		/* the bind takes care of ensuring only one copy running */
		retval = bind(sock, (struct sockaddr *) &sun, addrlen);
		if (retval < 0) {
			condlog(0, "bind failed, exit");
			goto exit;
		}

		/* enable receiving of the sender credentials */
		retval = setsockopt(sock, SOL_SOCKET, SO_PASSCRED,
				    &feature_on, sizeof(feature_on));
		if (retval < 0) {
			condlog(0, "failed to enable credential passing, exit");
			goto exit;
		}

	} else {
		/* Fallback to read kernel netlink events */
		memset(&snl, 0x00, sizeof(struct sockaddr_nl));
		snl.nl_family = AF_NETLINK;
		snl.nl_pid = getpid();
		snl.nl_groups = 0x01;

		sock = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
		if (sock == -1) {
			condlog(0, "error getting socket, exit");
			return 1;
		}

		condlog(3, "reading events from kernel.");

		/*
		 * try to avoid dropping uevents, even so, this is not a guarantee,
		 * but it does help to change the netlink uevent socket's
		 * receive buffer threshold from the default value of 106,496 to
		 * the maximum value of 262,142.
		 */
		retval = setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &rcvbufsz,
				    sizeof(rcvbufsz));

		if (retval < 0) {
			condlog(0, "error setting receive buffer size for socket, exit");
			exit(1);
		}
		retval = getsockopt(sock, SOL_SOCKET, SO_RCVBUF, &rcvsz, prcvszsz);
		if (retval < 0) {
			condlog(0, "error setting receive buffer size for socket, exit");
			exit(1);
		}
		condlog(3, "receive buffer size for socket is %u.", rcvsz);

		/* enable receiving of the sender credentials */
		if (setsockopt(sock, SOL_SOCKET, SO_PASSCRED,
			       &feature_on, sizeof(feature_on)) < 0) {
			condlog(0, "error on enabling credential passing for socket");
			exit(1);
		}

		retval = bind(sock, (struct sockaddr *) &snl,
			      sizeof(struct sockaddr_nl));
		if (retval < 0) {
			condlog(0, "bind failed, exit");
			goto exit;
		}
	}

	while (1) {
		size_t bufpos;
		ssize_t buflen;
		struct uevent *uev;
		struct msghdr smsg;
		struct iovec iov;
		char cred_msg[CMSG_SPACE(sizeof(struct ucred))];
		struct cmsghdr *cmsg;
		struct ucred *cred;
		static char buf[HOTPLUG_BUFFER_SIZE + OBJECT_SIZE];

		memset(buf, 0x00, sizeof(buf));
		iov.iov_base = &buf;
		iov.iov_len = sizeof(buf);
		memset (&smsg, 0x00, sizeof(struct msghdr));
		smsg.msg_iov = &iov;
		smsg.msg_iovlen = 1;
		smsg.msg_control = cred_msg;
		smsg.msg_controllen = sizeof(cred_msg);

		buflen = recvmsg(sock, &smsg, 0);
		if (buflen < 0) {
			if (errno != EINTR)
				condlog(0, "error receiving message, errno %d", errno);
			continue;
		}

		cmsg = CMSG_FIRSTHDR(&smsg);
		if (cmsg == NULL || cmsg->cmsg_type != SCM_CREDENTIALS) {
			condlog(3, "no sender credentials received, message ignored");
			continue;
		}

		cred = (struct ucred *)CMSG_DATA(cmsg);
		if (cred->uid != 0) {
			condlog(3, "sender uid=%d, message ignored", cred->uid);
			continue;
		}

		/* skip header */
		bufpos = strlen(buf) + 1;
		if (bufpos < sizeof("a@/d") || bufpos >= sizeof(buf)) {
			condlog(3, "invalid message length");
			continue;
		}

		/* check message header */
		if (strstr(buf, "@/") == NULL) {
			condlog(3, "unrecognized message header");
			continue;
		}
		if ((size_t)buflen > sizeof(buf)-1) {
			condlog(2, "buffer overflow for received uevent");
			buflen = sizeof(buf)-1;
		}

		uev = uevent_from_buffer(buf, buflen);
		if (!uev)
			continue;
		/*
		 * Queue uevent and poke service pthread.
		 */
		pthread_mutex_lock(uevq_lockp);
		list_add_tail(&uev->node, &uevq);
		pthread_cond_signal(uev_condp);
		pthread_mutex_unlock(uevq_lockp);
	}

exit:
	close(sock);
	return 1;
}

struct uevent *uevent_from_udev_device(struct udev_device *dev)
{
	struct uevent *uev;
	int i = 0;
	char *pos, *end;
	struct udev_list_entry *list_entry;

	uev = alloc_uevent();
	if (!uev) {
		udev_device_unref(dev);
		condlog(1, "lost uevent, oom");
		return NULL;
	}
	pos = uev->buffer;
	end = pos + HOTPLUG_BUFFER_SIZE + OBJECT_SIZE - 1;
	udev_list_entry_foreach(list_entry, udev_device_get_properties_list_entry(dev)) {
		const char *name, *value;
		int bytes;

		name = udev_list_entry_get_name(list_entry);
		if (!name)
			name = "(null)";
		value = udev_list_entry_get_value(list_entry);
		if (!value)
			value = "(null)";
		bytes = snprintf(pos, end - pos, "%s=%s", name, value);
		if (pos + bytes >= end) {
			condlog(2, "buffer overflow for uevent");
			break;
		}
		uev->envp[i] = pos;
		pos += bytes;
		*pos = '\0';
		pos++;
		if (strcmp(name, "DEVPATH") == 0)
			uev->devpath = uev->envp[i] + 8;
		if (strcmp(name, "ACTION") == 0)
			uev->action = uev->envp[i] + 7;
		i++;
		if (i == HOTPLUG_NUM_ENVP - 1)
			break;
	}
	uev->udev = dev;
	uev->envp[i] = NULL;

	condlog(3, "uevent '%s' from '%s'", uev->action, uev->devpath);
	uev->kernel = strrchr(uev->devpath, '/');
	if (uev->kernel)
		uev->kernel++;

	/* print payload environment */
	for (i = 0; uev->envp[i] != NULL; i++)
		condlog(5, "%s", uev->envp[i]);
	return uev;
}

bool uevent_burst(struct timeval *start_time, int events)
{
	struct timeval diff_time, end_time;
	unsigned long speed;
	unsigned long eclipse_ms;

	if(events > MAX_ACCUMULATION_COUNT) {
		condlog(2, "burst got %u uevents, too much uevents, stopped", events);
		return false;
	}

	gettimeofday(&end_time, NULL);
	timersub(&end_time, start_time, &diff_time);

	eclipse_ms = diff_time.tv_sec * 1000 + diff_time.tv_usec / 1000;

	if (eclipse_ms == 0)
		return true;

	if (eclipse_ms > MAX_ACCUMULATION_TIME) {
		condlog(2, "burst continued %lu ms, too long time, stopped", eclipse_ms);
		return false;
	}

	speed = (events * 1000) / eclipse_ms;
	if (speed > MIN_BURST_SPEED)
		return true;

	return false;
}

int uevent_listen(struct udev *udev)
{
	int err = 2;
	struct udev_monitor *monitor = NULL;
	int fd, socket_flags, events;
	struct timeval start_time;
	int need_failback = 1;
	int timeout = 30;
	LIST_HEAD(uevlisten_tmp);

	/*
	 * Queue uevents for service by dedicated thread so that the uevent
	 * listening thread does not block on multipathd locks (vecs->lock)
	 * thereby not getting to empty the socket's receive buffer queue
	 * often enough.
	 */
	if (!udev) {
		condlog(1, "no udev context");
		return 1;
	}
	udev_ref(udev);
	pthread_cleanup_push(uevent_cleanup, udev);

	monitor = udev_monitor_new_from_netlink(udev, "udev");
	if (!monitor) {
		condlog(2, "failed to create udev monitor");
		goto out;
	}
#ifdef LIBUDEV_API_RECVBUF
	if (udev_monitor_set_receive_buffer_size(monitor, 128 * 1024 * 1024))
		condlog(2, "failed to increase buffer size");
#endif
	fd = udev_monitor_get_fd(monitor);
	if (fd < 0) {
		condlog(2, "failed to get monitor fd");
		goto out;
	}
	socket_flags = fcntl(fd, F_GETFL);
	if (socket_flags < 0) {
		condlog(2, "failed to get monitor socket flags : %s",
			strerror(errno));
		goto out;
	}
	if (fcntl(fd, F_SETFL, socket_flags & ~O_NONBLOCK) < 0) {
		condlog(2, "failed to set monitor socket flags : %s",
			strerror(errno));
		goto out;
	}
	err = udev_monitor_filter_add_match_subsystem_devtype(monitor, "block",
							      NULL);
	if (err)
		condlog(2, "failed to create filter : %s", strerror(-err));
	err = udev_monitor_enable_receiving(monitor);
	if (err) {
		condlog(2, "failed to enable receiving : %s", strerror(-err));
		goto out;
	}

	events = 0;
	gettimeofday(&start_time, NULL);
	while (1) {
		struct uevent *uev;
		struct udev_device *dev;
		struct pollfd ev_poll;
		int poll_timeout;
		int fdcount;

		memset(&ev_poll, 0, sizeof(struct pollfd));
		ev_poll.fd = fd;
		ev_poll.events = POLLIN;
		poll_timeout = timeout * 1000;
		errno = 0;
		fdcount = poll(&ev_poll, 1, poll_timeout);
		if (fdcount && ev_poll.revents & POLLIN) {
			timeout = uevent_burst(&start_time, events + 1) ? 1 : 0;
			dev = udev_monitor_receive_device(monitor);
			if (!dev) {
				condlog(0, "failed getting udev device");
				continue;
			}
			uev = uevent_from_udev_device(dev);
			if (!uev)
				continue;
			list_add_tail(&uev->node, &uevlisten_tmp);
			events++;
			continue;
		}
		if (fdcount < 0) {
			if (errno == EINTR)
				continue;

			condlog(0, "error receiving "
				"uevent message: %m");
			err = -errno;
			break;
		}
		if (!list_empty(&uevlisten_tmp)) {
			/*
			 * Queue uevents and poke service pthread.
			 */
			condlog(3, "Forwarding %d uevents", events);
			pthread_mutex_lock(uevq_lockp);
			list_splice_tail_init(&uevlisten_tmp, &uevq);
			pthread_cond_signal(uev_condp);
			pthread_mutex_unlock(uevq_lockp);
			events = 0;
		}
		gettimeofday(&start_time, NULL);
		timeout = 30;
	}
	need_failback = 0;
out:
	if (monitor)
		udev_monitor_unref(monitor);
	if (need_failback)
		err = failback_listen();
	pthread_cleanup_pop(1);
	return err;
}

int uevent_get_major(struct uevent *uev)
{
	char *p, *q;
	int i, major = -1;

	for (i = 0; uev->envp[i] != NULL; i++) {
		if (!strncmp(uev->envp[i], "MAJOR", 5) && strlen(uev->envp[i]) > 6) {
			p = uev->envp[i] + 6;
			major = strtoul(p, &q, 10);
			if (p == q) {
				condlog(2, "invalid major '%s'", p);
				major = -1;
			}
			break;
		}
	}
	return major;
}

int uevent_get_minor(struct uevent *uev)
{
	char *p, *q;
	int i, minor = -1;

	for (i = 0; uev->envp[i] != NULL; i++) {
		if (!strncmp(uev->envp[i], "MINOR", 5) && strlen(uev->envp[i]) > 6) {
			p = uev->envp[i] + 6;
			minor = strtoul(p, &q, 10);
			if (p == q) {
				condlog(2, "invalid minor '%s'", p);
				minor = -1;
			}
			break;
		}
	}
	return minor;
}

int uevent_get_disk_ro(struct uevent *uev)
{
	char *p, *q;
	int i, ro = -1;

	for (i = 0; uev->envp[i] != NULL; i++) {
		if (!strncmp(uev->envp[i], "DISK_RO", 6) && strlen(uev->envp[i]) > 7) {
			p = uev->envp[i] + 8;
			ro = strtoul(p, &q, 10);
			if (p == q) {
				condlog(2, "invalid read_only setting '%s'", p);
				ro = -1;
			}
			break;
		}
	}
	return ro;
}

char *uevent_get_dm_name(struct uevent *uev)
{
	char *p = NULL;
	int i;

	for (i = 0; uev->envp[i] != NULL; i++) {
		if (!strncmp(uev->envp[i], "DM_NAME", 6) &&
		    strlen(uev->envp[i]) > 7) {
			p = MALLOC(strlen(uev->envp[i] + 8) + 1);
			strcpy(p, uev->envp[i] + 8);
			break;
		}
	}
	return p;
}
