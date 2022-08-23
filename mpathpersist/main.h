static struct option long_options[] = {
	{"verbose", 1, NULL, 'v'},
	{"clear", 0, NULL, 'C'},
	{"device", 1, NULL, 'd'},
	{"help", 0, NULL, 'h'},
	{"hex", 0, NULL, 'H'},
	{"in", 0, NULL, 'i'},
	{"out", 0, NULL, 'o'},
	{"param-aptpl", 0, NULL, 'Z'},
	{"param-rk", 1, NULL, 'K'},
	{"param-sark", 1, NULL, 'S'},
	{"preempt", 0, NULL, 'P'},
	{"preempt-abort", 0, NULL, 'A'},
	{"prout-type", 1, NULL, 'T'},
	{"read-full-status", 0, NULL, 's'},
	{"read-keys", 0, NULL, 'k'},
	{"read-reservation", 0, NULL, 'r'},
	{"register", 0, NULL, 'G'},
	{"register-ignore", 0, NULL, 'I'},
	{"release", 0, NULL, 'L'},
	{"report-capabilities", 0, NULL, 'c'},
	{"reserve", 0, NULL, 'R'},
	{"transport-id", 1, NULL, 'X'},
	{"alloc-length", 1, NULL, 'l'},
	{NULL, 0, NULL, 0}
};

static void usage(void);
