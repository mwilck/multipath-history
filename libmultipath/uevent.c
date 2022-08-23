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
 *	with this program; if not, write to the Free Software Foundation, Inc.,
 *	675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/user.h>
#include <sys/un.h>
#include <linux/types.h>
#include <linux/netlink.h>
#include <pthread.h>
#include <sys/mman.h>

#include "memory.h"
#include "debug.h"
#include "uevent.h"

typedef int (uev_trigger)(struct uevent *, void * trigger_data);

pthread_t uevq_thr;
struct uevent *uevqhp, *uevqtp;
pthread_mutex_t uevq_lock, *uevq_lockp = &uevq_lock;
pthread_mutex_t uevc_lock, *uevc_lockp = &uevc_lock;
pthread_cond_t  uev_cond,  *uev_condp  = &uev_cond;
uev_trigger *my_uev_trigger;
void * my_trigger_data;

static struct uevent * alloc_uevent (void)
{
	return (struct uevent *)MALLOC(sizeof(struct uevent));
}

void
service_uevq(void)
{
	int empty;
	struct uevent *uev;

	do {
		pthread_mutex_lock(uevq_lockp);
		empty = (uevqhp == NULL);
		if (!empty) {
			uev = uevqhp;
			uevqhp = uev->next;
			if (uevqtp == uev)
				uevqtp = uev->next;
			pthread_mutex_unlock(uevq_lockp);

			if (my_uev_trigger && my_uev_trigger(uev,
							my_trigger_data))
				condlog(0, "uevent trigger error");

			FREE(uev);
		}
		else {
			pthread_mutex_unlock(uevq_lockp);
		}
	} while (empty == 0);
}

/*
 * Service the uevent queue.
 */
static void *
uevq_thread(void * et)
{
	mlockall(MCL_CURRENT | MCL_FUTURE);

	while (1) {
		pthread_mutex_lock(uevc_lockp);
		pthread_cond_wait(uev_condp, uevc_lockp);
		pthread_mutex_unlock(uevc_lockp);

		service_uevq();
	}
	return NULL;
}

int uevent_listen(int (*uev_trigger)(struct uevent *, void * trigger_data),
		  void * trigger_data)
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
	pthread_attr_t attr;
	const int feature_on = 1;

	my_uev_trigger = uev_trigger;
	my_trigger_data = trigger_data;

	/*
	 * Queue uevents for service by dedicated thread so that the uevent
	 * listening thread does not block on multipathd locks (vecs->lock)
	 * thereby not getting to empty the socket's receive buffer queue
	 * often enough.
	 */
	uevqhp = uevqtp = NULL;

	pthread_mutex_init(uevq_lockp, NULL);
	pthread_mutex_init(uevc_lockp, NULL);
	pthread_cond_init(uev_condp, NULL);

	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 64 * 1024);
	pthread_create(&uevq_thr, &attr, uevq_thread, NULL);

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
		setsockopt(sock, SOL_SOCKET, SO_PASSCRED,
			   &feature_on, sizeof(feature_on));

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
		setsockopt(sock, SOL_SOCKET, SO_PASSCRED,
			   &feature_on, sizeof(feature_on));

		retval = bind(sock, (struct sockaddr *) &snl,
			      sizeof(struct sockaddr_nl));
		if (retval < 0) {
			condlog(0, "bind failed, exit");
			goto exit;
		}
	}

	while (1) {
		int i;
		char *pos;
		size_t bufpos;
		ssize_t buflen;
		struct uevent *uev;
		char *buffer;
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
				condlog(0, "error receiving message");
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

		uev = alloc_uevent();

		if (!uev) {
			condlog(1, "lost uevent, oom");
			continue;
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
			continue;
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
			bufpos += keylen + 1;
		}
		uev->envp[i] = NULL;

		condlog(3, "uevent '%s' from '%s'", uev->action, uev->devpath);

		/* print payload environment */
		for (i = 0; uev->envp[i] != NULL; i++)
			condlog(3, "%s", uev->envp[i]);

		/*
		 * Queue uevent and poke service pthread.
		 */
		pthread_mutex_lock(uevq_lockp);
		if (uevqtp)
			uevqtp->next = uev;
		else
			uevqhp = uev;
		uevqtp = uev;
		uev->next = NULL;
		pthread_mutex_unlock(uevq_lockp);

		pthread_mutex_lock(uevc_lockp);
		pthread_cond_signal(uev_condp);
		pthread_mutex_unlock(uevc_lockp);
	}

exit:
	close(sock);

	pthread_mutex_lock(uevq_lockp);
	pthread_cancel(uevq_thr);
	pthread_mutex_unlock(uevq_lockp);

	pthread_mutex_destroy(uevq_lockp);
	pthread_mutex_destroy(uevc_lockp);
	pthread_cond_destroy(uev_condp);

	return 1;
}
