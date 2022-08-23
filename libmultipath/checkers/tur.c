/*
 * Some code borrowed from sg-utils.
 *
 * Copyright (c) 2004 Christophe Varoqui
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/sysmacros.h>
#include <errno.h>
#include <sys/time.h>
#include <pthread.h>

#include "checkers.h"

#include "../libmultipath/debug.h"
#include "../libmultipath/sg_include.h"
#include "../libmultipath/util.h"
#include "../libmultipath/time-util.h"
#include "../libmultipath/util.h"

#define TUR_CMD_LEN 6
#define HEAVY_CHECK_COUNT       10

#define MSG_TUR_UP	"tur checker reports path is up"
#define MSG_TUR_DOWN	"tur checker reports path is down"
#define MSG_TUR_GHOST	"tur checker reports path is in standby state"
#define MSG_TUR_RUNNING	"tur checker still running"
#define MSG_TUR_TIMEOUT	"tur checker timed out"
#define MSG_TUR_FAILED	"tur checker failed to initialize"

struct tur_checker_context {
	dev_t devt;
	int state;
	int running;
	int fd;
	unsigned int timeout;
	time_t time;
	pthread_t thread;
	pthread_mutex_t lock;
	pthread_cond_t active;
	pthread_spinlock_t hldr_lock;
	int holders;
	char message[CHECKER_MSG_LEN];
};

static const char *tur_devt(char *devt_buf, int size,
			    struct tur_checker_context *ct)
{
	dev_t devt;

	pthread_mutex_lock(&ct->lock);
	devt = ct->devt;
	pthread_mutex_unlock(&ct->lock);

	snprintf(devt_buf, size, "%d:%d", major(devt), minor(devt));
	return devt_buf;
}

int libcheck_init (struct checker * c)
{
	struct tur_checker_context *ct;
	pthread_mutexattr_t attr;

	ct = malloc(sizeof(struct tur_checker_context));
	if (!ct)
		return 1;
	memset(ct, 0, sizeof(struct tur_checker_context));

	ct->state = PATH_UNCHECKED;
	ct->fd = -1;
	ct->holders = 1;
	pthread_cond_init_mono(&ct->active);
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&ct->lock, &attr);
	pthread_mutexattr_destroy(&attr);
	pthread_spin_init(&ct->hldr_lock, PTHREAD_PROCESS_PRIVATE);
	c->context = ct;

	return 0;
}

static void cleanup_context(struct tur_checker_context *ct)
{
	pthread_mutex_destroy(&ct->lock);
	pthread_cond_destroy(&ct->active);
	pthread_spin_destroy(&ct->hldr_lock);
	free(ct);
}

void libcheck_free (struct checker * c)
{
	if (c->context) {
		struct tur_checker_context *ct = c->context;
		int holders;
		pthread_t thread;

		pthread_spin_lock(&ct->hldr_lock);
		ct->holders--;
		holders = ct->holders;
		thread = ct->thread;
		pthread_spin_unlock(&ct->hldr_lock);
		if (holders)
			pthread_cancel(thread);
		else
			cleanup_context(ct);
		c->context = NULL;
	}
	return;
}

void libcheck_repair (struct checker * c)
{
	return;
}

#define TUR_MSG(fmt, args...)					\
	do {							\
		char msg[CHECKER_MSG_LEN];			\
								\
		snprintf(msg, sizeof(msg), fmt, ##args);	\
		copy_message(cb_arg, msg);			\
	} while (0)

static int
tur_check(int fd, unsigned int timeout,
	  void (*copy_message)(void *, const char *), void *cb_arg)
{
	struct sg_io_hdr io_hdr;
	unsigned char turCmdBlk[TUR_CMD_LEN] = { 0x00, 0, 0, 0, 0, 0 };
	unsigned char sense_buffer[32];
	int retry_tur = 5;

retry:
	memset(&io_hdr, 0, sizeof (struct sg_io_hdr));
	memset(&sense_buffer, 0, 32);
	io_hdr.interface_id = 'S';
	io_hdr.cmd_len = sizeof (turCmdBlk);
	io_hdr.mx_sb_len = sizeof (sense_buffer);
	io_hdr.dxfer_direction = SG_DXFER_NONE;
	io_hdr.cmdp = turCmdBlk;
	io_hdr.sbp = sense_buffer;
	io_hdr.timeout = timeout * 1000;
	io_hdr.pack_id = 0;
	if (ioctl(fd, SG_IO, &io_hdr) < 0) {
		TUR_MSG(MSG_TUR_DOWN);
		return PATH_DOWN;
	}
	if ((io_hdr.status & 0x7e) == 0x18) {
		/*
		 * SCSI-3 arrays might return
		 * reservation conflict on TUR
		 */
		TUR_MSG(MSG_TUR_UP);
		return PATH_UP;
	}
	if (io_hdr.info & SG_INFO_OK_MASK) {
		int key = 0, asc, ascq;

		switch (io_hdr.host_status) {
		case DID_OK:
		case DID_NO_CONNECT:
		case DID_BAD_TARGET:
		case DID_ABORT:
		case DID_TRANSPORT_FAILFAST:
			break;
		default:
			/* Driver error, retry */
			if (--retry_tur)
				goto retry;
			break;
		}
		if (io_hdr.sb_len_wr > 3) {
			if (io_hdr.sbp[0] == 0x72 || io_hdr.sbp[0] == 0x73) {
				key = io_hdr.sbp[1] & 0x0f;
				asc = io_hdr.sbp[2];
				ascq = io_hdr.sbp[3];
			} else if (io_hdr.sb_len_wr > 13 &&
				   ((io_hdr.sbp[0] & 0x7f) == 0x70 ||
				    (io_hdr.sbp[0] & 0x7f) == 0x71)) {
				key = io_hdr.sbp[2] & 0x0f;
				asc = io_hdr.sbp[12];
				ascq = io_hdr.sbp[13];
			}
		}
		if (key == 0x6) {
			/* Unit Attention, retry */
			if (--retry_tur)
				goto retry;
		}
		else if (key == 0x2) {
			/* Not Ready */
			/* Note: Other ALUA states are either UP or DOWN */
			if( asc == 0x04 && ascq == 0x0b){
				/*
				 * LOGICAL UNIT NOT ACCESSIBLE,
				 * TARGET PORT IN STANDBY STATE
				 */
				TUR_MSG(MSG_TUR_GHOST);
				return PATH_GHOST;
			}
		}
		TUR_MSG(MSG_TUR_DOWN);
		return PATH_DOWN;
	}
	TUR_MSG(MSG_TUR_UP);
	return PATH_UP;
}

#define tur_thread_cleanup_push(ct) pthread_cleanup_push(cleanup_func, ct)
#define tur_thread_cleanup_pop(ct) pthread_cleanup_pop(1)

static void cleanup_func(void *data)
{
	int holders;
	struct tur_checker_context *ct = data;
	pthread_spin_lock(&ct->hldr_lock);
	ct->holders--;
	holders = ct->holders;
	ct->thread = 0;
	pthread_spin_unlock(&ct->hldr_lock);
	if (!holders)
		cleanup_context(ct);
}

static int tur_running(struct tur_checker_context *ct)
{
	pthread_t thread;

	pthread_spin_lock(&ct->hldr_lock);
	thread = ct->thread;
	pthread_spin_unlock(&ct->hldr_lock);

	return thread != 0;
}

static void copy_msg_to_tcc(void *ct_p, const char *msg)
{
	struct tur_checker_context *ct = ct_p;

	pthread_mutex_lock(&ct->lock);
	strlcpy(ct->message, msg, sizeof(ct->message));
	pthread_mutex_unlock(&ct->lock);
}

static void *tur_thread(void *ctx)
{
	struct tur_checker_context *ct = ctx;
	int state;
	char devt[32];

	condlog(3, "%s: tur checker starting up",
		tur_devt(devt, sizeof(devt), ct));

	/* This thread can be canceled, so setup clean up */
	tur_thread_cleanup_push(ct);

	/* TUR checker start up */
	pthread_mutex_lock(&ct->lock);
	ct->state = PATH_PENDING;
	ct->message[0] = '\0';
	pthread_mutex_unlock(&ct->lock);

	state = tur_check(ct->fd, ct->timeout, copy_msg_to_tcc, ct->message);
	pthread_testcancel();

	/* TUR checker done */
	pthread_mutex_lock(&ct->lock);
	ct->state = state;
	pthread_cond_signal(&ct->active);
	pthread_mutex_unlock(&ct->lock);

	condlog(3, "%s: tur checker finished, state %s",
		tur_devt(devt, sizeof(devt), ct), checker_state_name(state));
	tur_thread_cleanup_pop(ct);

	return ((void *)0);
}


static void tur_timeout(struct timespec *tsp)
{
	clock_gettime(CLOCK_MONOTONIC, tsp);
	tsp->tv_nsec += 1000 * 1000; /* 1 millisecond */
	normalize_timespec(tsp);
}

static void tur_set_async_timeout(struct checker *c)
{
	struct tur_checker_context *ct = c->context;
	struct timespec now;

	clock_gettime(CLOCK_MONOTONIC, &now);
	ct->time = now.tv_sec + c->timeout;
}

static int tur_check_async_timeout(struct checker *c)
{
	struct tur_checker_context *ct = c->context;
	struct timespec now;

	clock_gettime(CLOCK_MONOTONIC, &now);
	return (now.tv_sec > ct->time);
}

static void copy_msg_to_checker(void *c_p, const char *msg)
{
	struct checker *c = c_p;

	strlcpy(c->message, msg, sizeof(c->message));
}

int libcheck_check(struct checker * c)
{
	struct tur_checker_context *ct = c->context;
	struct timespec tsp;
	struct stat sb;
	pthread_attr_t attr;
	int tur_status, r;
	char devt[32];


	if (!ct)
		return PATH_UNCHECKED;

	if (fstat(c->fd, &sb) == 0) {
		pthread_mutex_lock(&ct->lock);
		ct->devt = sb.st_rdev;
		pthread_mutex_unlock(&ct->lock);
	}

	if (c->sync)
		return tur_check(c->fd, c->timeout, copy_msg_to_checker, c);

	/*
	 * Async mode
	 */
	r = pthread_mutex_lock(&ct->lock);
	if (r != 0) {
		condlog(2, "%s: tur mutex lock failed with %d",
			tur_devt(devt, sizeof(devt), ct), r);
		MSG(c, MSG_TUR_FAILED);
		return PATH_WILD;
	}

	if (ct->running) {
		/*
		 * Check if TUR checker is still running. Hold hldr_lock
		 * around the pthread_cancel() call to avoid that
		 * pthread_cancel() gets called after the (detached) TUR
		 * thread has exited.
		 */
		pthread_spin_lock(&ct->hldr_lock);
		if (ct->thread) {
			if (tur_check_async_timeout(c)) {
				condlog(3, "%s: tur checker timeout",
					tur_devt(devt, sizeof(devt), ct));
				pthread_cancel(ct->thread);
				ct->running = 0;
				MSG(c, MSG_TUR_TIMEOUT);
				tur_status = PATH_TIMEOUT;
			} else {
				condlog(3, "%s: tur checker not finished",
					tur_devt(devt, sizeof(devt), ct));
				ct->running++;
				tur_status = PATH_PENDING;
			}
		} else {
			/* TUR checker done */
			ct->running = 0;
			tur_status = ct->state;
			strlcpy(c->message, ct->message, sizeof(c->message));
		}
		pthread_spin_unlock(&ct->hldr_lock);
		pthread_mutex_unlock(&ct->lock);
	} else {
		if (tur_running(ct)) {
			/* pthread cancel failed. continue in sync mode */
			pthread_mutex_unlock(&ct->lock);
			condlog(3, "%s: tur thread not responding",
				tur_devt(devt, sizeof(devt), ct));
			return PATH_TIMEOUT;
		}
		/* Start new TUR checker */
		ct->state = PATH_UNCHECKED;
		ct->fd = c->fd;
		ct->timeout = c->timeout;
		pthread_spin_lock(&ct->hldr_lock);
		ct->holders++;
		pthread_spin_unlock(&ct->hldr_lock);
		tur_set_async_timeout(c);
		setup_thread_attr(&attr, 32 * 1024, 1);
		r = pthread_create(&ct->thread, &attr, tur_thread, ct);
		pthread_attr_destroy(&attr);
		if (r) {
			pthread_spin_lock(&ct->hldr_lock);
			ct->holders--;
			pthread_spin_unlock(&ct->hldr_lock);
			pthread_mutex_unlock(&ct->lock);
			ct->thread = 0;
			condlog(3, "%s: failed to start tur thread, using"
				" sync mode", tur_devt(devt, sizeof(devt), ct));
			return tur_check(c->fd, c->timeout,
					 copy_msg_to_checker, c);
		}
		tur_timeout(&tsp);
		r = pthread_cond_timedwait(&ct->active, &ct->lock, &tsp);
		tur_status = ct->state;
		strlcpy(c->message, ct->message, sizeof(c->message));
		pthread_mutex_unlock(&ct->lock);
		if (tur_running(ct) &&
		    (tur_status == PATH_PENDING || tur_status == PATH_UNCHECKED)) {
			condlog(3, "%s: tur checker still running",
				tur_devt(devt, sizeof(devt), ct));
			ct->running = 1;
			tur_status = PATH_PENDING;
		}
	}

	return tur_status;
}
