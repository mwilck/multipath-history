#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <errno.h>
#include "debug.h"

#define PROGRAM_SIZE	100
#define FIELD_PROGRAM

#define strfieldcpy(to, from) \
do { \
	to[sizeof(to)-1] = '\0'; \
	strncpy(to, from, sizeof(to)-1); \
} while (0)

int execute_program(char *path, char *value, int len)
{
	int retval;
	int count;
	int status;
	int fds[2];
	pid_t pid;
	char *pos;
	char arg[PROGRAM_SIZE];
	char *argv[sizeof(arg) / 2];
	int i;

	i = 0;
	if (strchr(path, ' ')) {
		strfieldcpy(arg, path);
		pos = arg;
		while (pos != NULL) {
			if (pos[0] == '\'') {
				/* don't separate if in apostrophes */
				pos++;
				argv[i] = strsep(&pos, "\'");
				while (pos[0] == ' ')
					pos++;
		} else {
				argv[i] = strsep(&pos, " ");
			}
			dbg("arg[%i] '%s'", i, argv[i]);
			i++;
		}
	}
	argv[i] =  NULL;

	retval = pipe(fds);
	if (retval != 0) {
		dbg("pipe failed");
		return -1;
	}

	pid = fork();
	switch(pid) {
	case 0:
		/* child */
		close(STDOUT_FILENO);

		/* dup write side of pipe to STDOUT */
		dup(fds[1]);
		if (argv[0] !=  NULL) {
			dbg("execute '%s' with given arguments", argv[0]);
			retval = execv(argv[0], argv);
		} else {
			dbg("execute '%s' with main argument", path);
			//retval = execv(path, main_argv);
			retval = execv(path, NULL);
		}

		dbg(FIELD_PROGRAM " execution of '%s' failed", path);
		exit(1);
	case -1:
		dbg("fork failed");
		return -1;
	default:
		/* parent reads from fds[0] */
		close(fds[1]);
		retval = 0;
		i = 0;
		while (1) {
			count = read(fds[0], value + i, len - i-1);
			if (count <= 0)
				break;

			i += count;
			if (i >= len-1) {
				dbg("result len %d too short", len);
				retval = -1;
				break;
			}
		}

		if (count < 0) {
			dbg("read failed with '%s'", strerror(errno));
			retval = -1;
		}

		if (i > 0 && value[i-1] == '\n')
			i--;
		value[i] = '\0';
		dbg("result is '%s'", value);

		close(fds[0]);
		wait(&status);

		if (!WIFEXITED(status) || (WEXITSTATUS(status) != 0)) {
			dbg("exec program status 0x%x", status);
			retval = -1;
		}
	}
	return retval;
}
