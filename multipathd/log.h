#ifndef LOG_H
#define LOG_H

#define DEFAULT_AREA_SIZE 2048
#define MAX_MSG_SIZE 128

#ifndef LOGLEVEL
#define LOGLEVEL 5
#endif

#if LOGDBG
#define logdbg(file, fmt, args...) fprintf(file, fmt, ##args)
#else
#define logdbg(file, fmt, args...) do {} while (0)
#endif

struct logmsg {
	short int prio;
	void * next;
	char * str;
};

struct logarea {
	int empty;
	void * head;
	void * tail;
	void * start;
	void * end;
};

struct logarea * la;

int log_init (char * progname, int size);
void log_close (void);
int log_enqueue (int prio, const char * fmt, va_list ap);
int log_dequeue (void *);
void log_syslog (void *);
char * log_alloc_buffer (void);
void dump_logmsg (void *);

#endif /* LOG_H */
