#ifndef _LOG_PTHREAD_H
#define _LOG_PTHREAD_H

#include <pthread.h>

pthread_t log_thr;

pthread_mutex_t *logq_lock;
pthread_mutex_t *logev_lock;
pthread_cond_t *logev_cond;

void log_safe(int prio, const char * fmt, va_list ap);
void log_thread_start(pthread_attr_t *attr);
void log_thread_stop(void);

#endif /* _LOG_PTHREAD_H */
