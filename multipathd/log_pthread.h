pthread_t log_thr;

pthread_mutex_t *logq_lock;
pthread_mutex_t *logev_lock;
pthread_cond_t *logev_cond;

void log_safe(int prio, char * fmt, ...);
void log_thread_start(void);
void log_thread_stop(void);
