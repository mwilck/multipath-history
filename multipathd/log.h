#ifndef _LOG_H
#define _LOG_H

#include <syslog.h>
#include <pthread.h>

#ifndef DEBUG
#define DEBUG 1
#endif

#define LOG(x, y, z...) if (DEBUG>=x) syslog(x, "[%lu] " y, pthread_self(), ##z)
#define safe_sprintf(var, format, args...)	\
	snprintf(var, sizeof(var), format, ##args) >= sizeof(var)
#define safe_snprintf(var, size, format, args...)      \
	snprintf(var, size, format, ##args) >= size

#endif /* _LOG_H */
