/* 
   Unix SMB/CIFS implementation.
   pidfile handling
   Copyright (C) Andrew Tridgell 1998
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <sys/types.h> /* for pid_t */
#include <signal.h>    /* for kill() */
#include <errno.h>     /* for ESHRC */
#include <stdio.h>     /* for f...() */
#include <string.h>    /* for memset() */
#include <stdlib.h>    /* for atoi() */
#include <unistd.h>    /* for unlink() */

#include <debug.h>

#include "pidfile.h"

/* Check if a process exists. */
static int process_exists(pid_t pid)
{
	if (pid > 0)
		return (kill(pid, 0) == 0 || errno != ESRCH);
	return 0;
}

/* Return the pid in a pidfile, or return 0 
 * if the process or pidfile does not exist. */
static pid_t pidfile_pid(const char *pidFile)
{
	FILE *f;
	char pidstr[20];
	unsigned ret;

	memset(pidstr, 0, sizeof(pidstr));

	if (!(f = fopen(pidFile, "r"))) {
		return 0;
	}
	if (fread(pidstr, sizeof(char), sizeof(pidstr)-1, f) <= 0) {
		goto noproc;
	}
	ret = atoi(pidstr);
	
	if (!process_exists((pid_t)ret)) {
		goto noproc;
	}
	fclose(f);
	return (pid_t)ret;

noproc:
	fclose(f);
	unlink(pidFile);
	return 0;
}

int pidfile_create(const char *pidFile, pid_t pid)
{
	FILE *f;
	char buf[20];
	pid_t oldpid;

	oldpid = pidfile_pid(pidFile);

	if (oldpid != 0) {
		condlog(0, "File [%s] exists and process id [%d] is running.", 
			pidFile, (int)pid);
		return 1;
	}
	if (!(f = fopen(pidFile, "w"))) {
		condlog(0, "Cannot open pidfile [%s], error was [%s]",
			pidFile, strerror(errno));
		return 1;
	}
	memset(buf, 0, sizeof(buf));
	snprintf(buf, sizeof(buf)-1, "%u", pid);
	fwrite(buf, sizeof(char), strlen(buf), f);
	fflush(f);
	fclose(f);

	return 0;
}

