/*
 * Original author : tridge@samba.org, January 2002
 * 
 * Copyright (c) 2005 Christophe Varoqui
 * Copyright (c) 2005 Benjamin Marzinski, Redhat
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/poll.h>
#include <readline/readline.h>
#include <readline/history.h>

#include <uxsock.h>
#include <memory.h>
#include <defaults.h>

#include <vector.h>
#include "cli.h"

/*
 * process the client 
 */
static void process(int fd)
{
	char *line;
	char *reply;

	cli_init();
	rl_readline_name = "multipathd";
	rl_completion_entry_function = key_generator;
	while ((line = readline("multipathd> "))) {
		size_t len;
		size_t llen = strlen(line);

		if (!llen) {
			free(line);
			continue;
		}

		if (send_packet(fd, line, llen + 1) != 0) break;
		if (recv_packet(fd, &reply, &len) != 0) break;

		printf("%s", reply);

		if (line && *line)
			add_history(line);

		free(line);
		FREE(reply);
	}
}

static void process_req(int fd, char * inbuf)
{
	char *reply;
	size_t len;

	send_packet(fd, inbuf, strlen(inbuf) + 1);
	recv_packet(fd, &reply, &len);

	printf("%s", reply);
	FREE(reply);
}
	
/*
 * entry point
 */
int uxclnt(char * inbuf)
{
	int fd;

	fd = ux_socket_connect(DEFAULT_SOCKET);
	if (fd == -1) {
		perror("ux_socket_connect");
		exit(1);
	}

	if (inbuf)
		process_req(fd, inbuf);
	else
		process(fd);
	
	return 0;
}
