/*
 * Copyright (c) 2004, 2005 Christophe Varoqui
 * Copyright (c) 2005 Kiyoshi Ueda, NEC
 * Copyright (c) 2005 Benjamin Marzinski, Redhat
 * Copyright (c) 2005 Edward Goggin, EMC
 */
#include <unistd.h>
#include <libdevmapper.h>
#include <sys/mman.h>
#include <pthread.h>
#include <signal.h>

#include "vector.h"
#include "memory.h"
#include "checkers.h"
#include "structs.h"
#include "structs_vec.h"
#include "devmapper.h"
#include "debug.h"
#include "lock.h"
#include "waiter.h"

pthread_attr_t waiter_attr;

struct event_thread *alloc_waiter (void)
{

	struct event_thread *wp;

	wp = (struct event_thread *)MALLOC(sizeof(struct event_thread));
	memset(wp, 0, sizeof(struct event_thread));
	pthread_mutex_init(&wp->lock, NULL);

	return wp;
}

void signal_waiter (void *data)
{
	struct event_thread *wp = (struct event_thread *)data;

	pthread_mutex_lock(&wp->lock);
	memset(wp->mapname, 0, WWID_SIZE);
	pthread_mutex_unlock(&wp->lock);
}

void free_waiter (struct event_thread *wp)
{
	pthread_mutex_destroy(&wp->lock);
	FREE(wp);
}

void stop_waiter_thread (struct multipath *mpp, struct vectors *vecs)
{
	if (mpp->waiter == (pthread_t)0) {
		condlog(3, "%s: event checker thread already stopped",
			mpp->alias);
		return;
	}
	condlog(2, "%s: stop event checker thread (%lu)", mpp->alias,
		mpp->waiter);
	pthread_kill(mpp->waiter, SIGUSR1);
	mpp->waiter = (pthread_t)0;
}

static sigset_t unblock_signals(void)
{
	sigset_t set, old;

	sigemptyset(&set);
	sigaddset(&set, SIGHUP);
	sigaddset(&set, SIGUSR1);
	pthread_sigmask(SIG_UNBLOCK, &set, &old);
	return old;
}

/*
 * returns the reschedule delay
 * negative means *stop*
 */
int waiteventloop (struct event_thread *waiter)
{
	sigset_t set;
	struct dm_task *dmt = NULL;
	int event_nr;
	int r;

	pthread_mutex_lock(&waiter->lock);
	if (!waiter->event_nr)
		waiter->event_nr = dm_geteventnr(waiter->mapname);

	if (!(dmt = dm_task_create(DM_DEVICE_WAITEVENT))) {
		condlog(0, "%s: devmap event #%i dm_task_create error",
				waiter->mapname, waiter->event_nr);
		pthread_mutex_unlock(&waiter->lock);
		return 1;
	}

	if (!dm_task_set_name(dmt, waiter->mapname)) {
		condlog(0, "%s: devmap event #%i dm_task_set_name error",
				waiter->mapname, waiter->event_nr);
		dm_task_destroy(dmt);
		pthread_mutex_unlock(&waiter->lock);
		return 1;
	}

	if (waiter->event_nr && !dm_task_set_event_nr(dmt,
						      waiter->event_nr)) {
		condlog(0, "%s: devmap event #%i dm_task_set_event_nr error",
				waiter->mapname, waiter->event_nr);
		dm_task_destroy(dmt);
		pthread_mutex_unlock(&waiter->lock);
		return 1;
	}
	pthread_mutex_unlock(&waiter->lock);

	dm_task_no_open_count(dmt);

	/* accept wait interruption */
	set = unblock_signals();

	/* wait */
	r = dm_task_run(dmt);

	/* wait is over : event or interrupt */
	pthread_sigmask(SIG_SETMASK, &set, NULL);

	dm_task_destroy(dmt);

	if (!r)	/* wait interrupted by signal */
		return -1;

	pthread_mutex_lock(&waiter->lock);
	if (!strlen(waiter->mapname)) {
		/* waiter should exit */
		pthread_mutex_unlock(&waiter->lock);
		return -1;
	}
	waiter->event_nr++;

	/*
	 * upon event ...
	 */
	while (1) {
		condlog(3, "%s: devmap event #%i",
				waiter->mapname, waiter->event_nr);

		/*
		 * event might be :
		 *
		 * 1) a table reload, which means our mpp structure is
		 *    obsolete : refresh it through update_multipath()
		 * 2) a path failed by DM : mark as such through
		 *    update_multipath()
		 * 3) map has gone away : stop the thread.
		 * 4) a path reinstate : nothing to do
		 * 5) a switch group : nothing to do
		 */
		pthread_cleanup_push(cleanup_lock, &waiter->vecs->lock);
		lock(waiter->vecs->lock);
		r = update_multipath(waiter->vecs, waiter->mapname);
		lock_cleanup_pop(waiter->vecs->lock);

		if (r) {
			condlog(2, "%s: event checker exit",
				waiter->mapname);
			pthread_mutex_unlock(&waiter->lock);
			return -1; /* stop the thread */
		}

		event_nr = dm_geteventnr(waiter->mapname);

		if (waiter->event_nr == event_nr) {
			pthread_mutex_unlock(&waiter->lock);
			return 1; /* upon problem reschedule 1s later */
		}

		waiter->event_nr = event_nr;
	}
	pthread_mutex_unlock(&waiter->lock);
	return -1; /* never reach there */
}

void *waitevent (void *et)
{
	int r;
	struct event_thread *waiter;

	mlockall(MCL_CURRENT | MCL_FUTURE);

	waiter = (struct event_thread *)et;
	pthread_cleanup_push(signal_waiter, et);

	block_signal(SIGUSR1, NULL);
	block_signal(SIGHUP, NULL);
	while (1) {
		r = waiteventloop(waiter);

		if (r < 0)
			break;

		sleep(r);
	}

	pthread_cleanup_pop(1);
	free_waiter(waiter);
	return NULL;
}

int start_waiter_thread (struct multipath *mpp, struct vectors *vecs)
{
	struct event_thread *wp;

	if (!mpp)
		return 0;

	wp = alloc_waiter();

	if (!wp)
		goto out;

	pthread_mutex_lock(&wp->lock);
	strncpy(wp->mapname, mpp->alias, WWID_SIZE);
	wp->vecs = vecs;
	pthread_mutex_unlock(&wp->lock);

	if (pthread_create(&wp->thread, &waiter_attr, waitevent, wp)) {
		condlog(0, "%s: cannot create event checker", wp->mapname);
		goto out1;
	}
	mpp->waiter = wp->thread;
	condlog(2, "%s: event checker started", wp->mapname);

	return 0;
out1:
	free_waiter(wp);
	mpp->waiter = (pthread_t)0;
out:
	condlog(0, "failed to start waiter thread");
	return 1;
}

