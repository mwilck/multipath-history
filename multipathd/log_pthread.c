#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <pthread.h>
#include <sys/mman.h>

#include "log_pthread.h"
#include "log.h"

void log_safe (int prio, char * fmt, ...)
{
	va_list ap;

	pthread_mutex_lock(logq_lock);
	va_start(ap, fmt);
	log_enqueue(prio, fmt, ap);
	va_end(ap);
	pthread_mutex_unlock(logq_lock);

	pthread_mutex_lock(logev_lock);
	pthread_cond_signal(logev_cond);
	pthread_mutex_unlock(logev_lock);
}

void * log_thread (void * et)
{
	char * buff = log_alloc_buffer();
	int more = 1;

	if (!buff)
		exit(1);

	mlockall(MCL_CURRENT | MCL_FUTURE);
	logdbg(stderr,"enter log_thread\n");
	/*
	 * signal the caller we are ready to receive logs
	 */
	pthread_mutex_lock(logev_lock);
	pthread_cond_signal(logev_cond);
	pthread_mutex_unlock(logev_lock);

	while(1) {
		pthread_mutex_lock(logev_lock);
		pthread_cond_wait(logev_cond, logev_lock);
		pthread_mutex_unlock(logev_lock);
		logdbg(stderr,"log_thread loop awaken\n");

		while (more) {
			pthread_mutex_lock(logq_lock);
			more = log_dequeue(&buff);
			//dump_logmsg(&buff);
			pthread_mutex_unlock(logq_lock);

			log_syslog(&buff);
		}
		more = 1;
	}
}

void log_thread_start (void)
{
	pthread_t log_thr;
	pthread_attr_t attr;
	
	logdbg(stderr,"enter log_thread_start\n");

	logq_lock = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
	logev_lock = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
	logev_cond = (pthread_cond_t *) malloc(sizeof(pthread_cond_t));
	
	pthread_mutex_init(logq_lock, NULL);
	pthread_mutex_init(logev_lock, NULL);
	pthread_cond_init(logev_cond, NULL);
	
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 64 * 1024);

	if (log_init("multipathd", 0)) {
		fprintf(stderr,"can't initialize log buffer\n");
		exit(1);
	}
	pthread_create(&log_thr, &attr, log_thread, NULL);

	/*
	 * wait for the logger thread before returning
	 */
	pthread_mutex_lock(logev_lock);
	pthread_cond_wait(logev_cond, logev_lock);
	pthread_mutex_unlock(logev_lock);

	return;
}

void log_thread_stop (void)
{
	pthread_cond_signal(logev_cond);
}	
