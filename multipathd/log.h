#ifndef _LOG_H
#define _LOG_H

#include <syslog.h>
#include <pthread.h>

#ifndef DEBUG
#define DEBUG 1
#endif

#define LOG(x, y, z...) if (DEBUG>=x) syslog(x, "[%lu] " y, pthread_self(), ##z)

#endif /* _LOG_H */
