#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <syslog.h>

#include "log.h"

#if LOGDBG
static void dump_logarea (void)
{
	char * p;
	int i;

	for ((p = la->start) && (i = 0); (void *)p < la->end; p++ && i++) {
		if (*p >= 0x20)
			logdbg(stderr, "%c", *p);
		else if (*p == 0x0)
			logdbg(stderr, ".");
		else
			logdbg(stderr, "*");

		if ((i % 64) == 0) {
			i = 0;
			logdbg(stderr, "\n");
		}
	}
	logdbg(stderr, "\n");
}

void dump_logmsg (void * data)
{
	struct logmsg * msg = (struct logmsg *)data;
	char * p;
	int i;

	logdbg(stderr,"dump_logmsg: msg addr %p\n", data);

	for ((p = data) && (i = 0);
	     (void *)p < (data + MAX_MSG_SIZE + sizeof(struct logmsg));
	     p++ && i++) {
		if (*p >= 0x20)
			logdbg(stderr, "%c", *p);
		else if (*p == 0x0)
			logdbg(stderr, ".");
		else
			logdbg(stderr, "*");

		if ((i % 64) == 0) {
			i = 0;
			logdbg(stderr, "\n");
		}
	}
	logdbg(stderr, "dump message: msg->prio = %i\n", msg->prio);
	logdbg(stderr, "dump message: msg->next = %p\n", msg->next);
	logdbg(stderr, "dump message: msg->str  = %s\n", (char *)&msg->str);
}
#endif

static int logarea_init (int size)
{
	logdbg(stderr,"enter logarea_init\n");
	la = malloc(sizeof(struct logarea));
	
	if (!la)
		return 1;

	if (size < MAX_MSG_SIZE)
		size = DEFAULT_AREA_SIZE;

	la->start = malloc(size);
	memset(la->start, 0, size);

	if (!la->start)
		return 1;

	la->empty = 1;
	la->end = la->start + size;
	la->head = la->start;
	la->tail = la->start;

	return 0;
}

int log_init(char *program_name, int size)
{
	logdbg(stderr,"enter log_init\n");
	openlog(program_name, 0, LOG_DAEMON);
	setlogmask(LOG_UPTO(LOGLEVEL));

	if (logarea_init(size))
		return 1;

	return 0;
}

void free_logarea (void)
{
	free(la);
	return;
}

void log_close (void)
{
	free_logarea();
	closelog();

	return;
}

int log_enqueue (int prio, const char * fmt, va_list ap)
{
	int len, fwd;
	char buff[MAX_MSG_SIZE];
	struct logmsg * msg;
	struct logmsg * lastmsg;

	lastmsg = (struct logmsg *)la->tail;

	if (!la->empty) {
		fwd = sizeof(struct logmsg) + 
		      strlen((char *)&lastmsg->str) * sizeof(char) + 1;
		la->tail += fwd;
	}
	vsnprintf(buff, MAX_MSG_SIZE, fmt, ap);
	len = strlen(buff) * sizeof(char) + 1;

	/* not enough space on tail : rewind */
	if (la->head <= la->tail &&
	    (len + sizeof(struct logmsg)) > (la->end - la->tail)) {
		logdbg(stderr, "enqueue: rewind tail to %p\n", la->tail);
		la->tail = la->start;
	}

	/* not enough space on head : drop msg */
	if (la->head > la->tail &&
	    (len + sizeof(struct logmsg)) > (la->head - la->tail)) {
		logdbg(stderr, "enqueue: log area overrun, drop msg\n");

		if (!la->empty)
			la->tail -= fwd;

		return 1;
	}

	/* ok, we can stage the msg in the area */
	la->empty = 0;
	msg = (struct logmsg *)la->tail;

	msg->prio = prio;
	logdbg(stderr, "enqueue: msg prio set to %i\n", msg->prio);
	
	memcpy((void *)&msg->str, buff, len);
	lastmsg->next = la->tail;
	msg->next = la->head;

#if LOGDBG
	dump_logarea();
#endif
	return 0;
}

int log_dequeue (void * buff)
{
	struct logmsg * src = (struct logmsg *)la->head;
	struct logmsg * dst = (struct logmsg *)buff;
	int len = strlen((char *)&src->str) * sizeof(char) +
		  sizeof(struct logmsg) + 1;

	if (!la->empty) {
		dst->prio = src->prio;
		memcpy(dst, src,  len);
		la->head = src->next;
		memset((void *)src, 0,  len);
		
		if (la->tail == la->head) {
			la->empty = 1;
			return 0;
		}
		return 1;
	}
	return 0;
}

/*
 * this one can block under memory pressure
 */
void log_syslog (void * buff)
{
	struct logmsg * msg = (struct logmsg *)buff;

	logdbg(stderr,"log_syslog: %s\n", (char *)&msg->str);
	syslog(msg->prio, "%s", (char *)&msg->str);
	memset(msg, 0, MAX_MSG_SIZE + sizeof(struct logmsg));
}

char * log_alloc_buffer (void)
{
	char * p = malloc(MAX_MSG_SIZE + sizeof(struct logmsg));

	if (p)
		memset(p, 0, MAX_MSG_SIZE + sizeof(struct logmsg));

	logdbg(stderr,"log_alloc_buffer: %p\n", p);

	return p;
}
