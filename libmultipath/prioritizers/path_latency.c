/*
 * (C) Copyright HUAWEI Technology Corp. 2017, All Rights Reserved.
 *
 * path_latency.c
 *
 * Prioritizer for device mapper multipath, where the corresponding priority
 * values of specific paths are provided by a latency algorithm. And the
 * latency algorithm is dependent on arguments("io_num" and "base_num").
 *
 * The principle of the algorithm as follows:
 * 1. By sending a certain number "io_num" of read IOs to the current path
 *    continuously, the IOs' average latency can be calculated.
 * 2. Max value and min value of average latency are constant. According to
 *    the average latency of each path and the "base_num" of logarithmic
 *    scale, the priority "rc" of each path can be provided.
 *
 * Author(s): Yang Feng <philip.yang@huawei.com>
 * Revised:   Guan Junxiong <guanjunxiong@huawei.com>
 *
 * This file is released under the GPL version 2, or any later version.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <math.h>
#include <ctype.h>
#include <time.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <unistd.h>

#include "debug.h"
#include "prio.h"
#include "structs.h"
#include "util.h"

#define pp_pl_log(prio, fmt, args...) condlog(prio, "path_latency prio: " fmt, ##args)

#define MAX_IO_NUM		200
#define MIN_IO_NUM		20
#define DEF_IO_NUM		100

#define MAX_BASE_NUM		10
#define MIN_BASE_NUM		1.01
#define DEF_BASE_NUM		1.5

#define MAX_AVG_LATENCY		100000000.	/* Unit: us */
#define MIN_AVG_LATENCY		1.		/* Unit: us */

#define DEFAULT_PRIORITY	0

#define USEC_PER_SEC		1000000LL
#define NSEC_PER_USEC		1000LL

#define DEF_BLK_SIZE		4096

static double lg_path_latency[MAX_IO_NUM];

static inline long long timeval_to_us(const struct timespec *tv)
{
	return ((long long)tv->tv_sec * USEC_PER_SEC) +
	    (tv->tv_nsec / NSEC_PER_USEC);
}

static int prepare_directio_read(int fd, int *blksz, char **pbuf,
		int *restore_flags)
{
	unsigned long pgsize = getpagesize();
	long flags;

	if (ioctl(fd, BLKBSZGET, blksz) < 0) {
		pp_pl_log(3,"catnnot get blocksize, set default");
		*blksz = DEF_BLK_SIZE;
	}
	if (posix_memalign((void **)pbuf, pgsize, *blksz))
		return -1;

	flags = fcntl(fd, F_GETFL);
	if (flags < 0)
		goto free_out;
	if (!(flags & O_DIRECT)) {
		flags |= O_DIRECT;
		if (fcntl(fd, F_SETFL, flags) < 0)
			goto free_out;
		*restore_flags = 1;
	}

	return 0;

free_out:
	free(*pbuf);

	return -1;
}

static void cleanup_directio_read(int fd, char *buf, int restore_flags)
{
	long flags;

	free(buf);

	if (!restore_flags)
		return;
	if ((flags = fcntl(fd, F_GETFL)) >= 0) {
		int ret __attribute__ ((unused));
		flags &= ~O_DIRECT;
		/* No point in checking for errors */
		ret = fcntl(fd, F_SETFL, flags);
	}
}

static int do_directio_read(int fd, unsigned int timeout, char *buf, int sz)
{
	fd_set read_fds;
	struct timeval tm = { .tv_sec = timeout };
	int ret;
	int num_read;

	if (lseek(fd, 0, SEEK_SET) == -1)
		return -1;
	FD_ZERO(&read_fds);
	FD_SET(fd, &read_fds);
	ret = select(fd+1, &read_fds, NULL, NULL, &tm);
	if (ret <= 0)
		return -1;
	num_read = read(fd, buf, sz);
	if (num_read != sz)
		return -1;

	return 0;
}

int check_args_valid(int io_num, double base_num)
{
	if ((io_num < MIN_IO_NUM) || (io_num > MAX_IO_NUM)) {
		pp_pl_log(0, "args io_num is outside the valid range");
		return 0;
	}

	if ((base_num < MIN_BASE_NUM) || (base_num > MAX_BASE_NUM)) {
		pp_pl_log(0, "args base_num is outside the valid range");
		return 0;
	}

	return 1;
}

/*
 * In multipath.conf, args form: io_num=n base_num=m. For example, args are
 * "io_num=20 base_num=10", this function can get io_num value 20 and
 * base_num value 10.
 */
static int get_ionum_and_basenum(char *args, int *ionum, double *basenum)
{
	char split_char[] = " \t";
	char *arg, *temp;
	char *str, *str_inval;
	int i;
	int flag_io = 0, flag_base = 0;

	if ((args == NULL) || (ionum == NULL) || (basenum == NULL)) {
		pp_pl_log(0, "args string is NULL");
		return 0;
	}

	arg = temp = STRDUP(args);
	if (!arg)
		return 0;

	for (i = 0; i < 2; i++) {
		str = get_next_string(&temp, split_char);
		if (!str)
			goto out;
		if (!strncmp(str, "io_num=", 7) && strlen(str) > 7) {
			*ionum = (int)strtoul(str + 7, &str_inval, 10);
			if (str == str_inval)
				goto out;
			flag_io = 1;
		}
		else if (!strncmp(str, "base_num=", 9) && strlen(str) > 9) {
			*basenum = strtod(str + 9, &str_inval);
			if (str == str_inval)
				goto out;
			flag_base = 1;
		}
	}

	if (!flag_io || !flag_base)
		goto out;
	if (check_args_valid(*ionum, *basenum) == 0)
		goto out;

	FREE(arg);
	return 1;
out:
	FREE(arg);
	return 0;
}

double calc_standard_deviation(double *lg_path_latency, int size,
				  double lg_avglatency)
{
	int index;
	double sum = 0;

	for (index = 0; index < size; index++) {
		sum += (lg_path_latency[index] - lg_avglatency) *
			(lg_path_latency[index] - lg_avglatency);
	}

	sum /= (size - 1);

	return sqrt(sum);
}

/*
 * Do not scale the prioriy in a certain range such as [0, 1024]
 * because scaling will eliminate the effect of base_num.
 */
int calcPrio(double lg_avglatency, double lg_maxavglatency,
		double lg_minavglatency)
{
	if (lg_avglatency <= lg_minavglatency)
		return lg_maxavglatency - lg_minavglatency;

	if (lg_avglatency >= lg_maxavglatency)
		return 0;

	return lg_maxavglatency - lg_avglatency;
}

int getprio(struct path *pp, char *args, unsigned int timeout)
{
	int rc, temp;
	int index = 0;
	int io_num = 0;
	double base_num = 0;
	double lg_avglatency, lg_maxavglatency, lg_minavglatency;
	double standard_deviation;
	double lg_toldelay = 0;
	long long before, after;
	struct timespec tv;
	int blksize;
	char *buf;
	int restore_flags = 0;
	double lg_base;
	long long sum_latency = 0;
	long long arith_mean_lat;

	if (pp->fd < 0)
		return -1;

	if (get_ionum_and_basenum(args, &io_num, &base_num) == 0) {
		io_num = DEF_IO_NUM;
		base_num = DEF_BASE_NUM;
		pp_pl_log(0, "%s: fails to get path_latency args, set default:"
				"io_num=%d base_num=%.3lf",
				pp->dev, io_num, base_num);
	}

	memset(lg_path_latency, 0, sizeof(lg_path_latency));
	lg_base = log(base_num);
	lg_maxavglatency = log(MAX_AVG_LATENCY) / lg_base;
	lg_minavglatency = log(MIN_AVG_LATENCY) / lg_base;

	prepare_directio_read(pp->fd, &blksize, &buf, &restore_flags);

	temp = io_num;
	while (temp-- > 0) {
		(void)clock_gettime(CLOCK_MONOTONIC, &tv);
		before = timeval_to_us(&tv);

		if (do_directio_read(pp->fd, timeout, buf, blksize)) {
			pp_pl_log(0, "%s: path down", pp->dev);
			cleanup_directio_read(pp->fd, buf, restore_flags);
			return -1;
		}

		(void)clock_gettime(CLOCK_MONOTONIC, &tv);
		after = timeval_to_us(&tv);
		/*
		 * We assume that the latency complies with Log-normal
		 * distribution. The logarithm of latency is in normal
		 * distribution.
		 */
		lg_path_latency[index] = log(after - before) / lg_base;
		lg_toldelay += lg_path_latency[index++];
		sum_latency += after - before;
	}

	cleanup_directio_read(pp->fd, buf, restore_flags);

	lg_avglatency = lg_toldelay / (long long)io_num;
	arith_mean_lat = sum_latency / (long long)io_num;
	pp_pl_log(4, "%s: arithmetic mean latency is (%lld us), geometric mean latency is (%lld us)",
			pp->dev, arith_mean_lat,
			(long long)pow(base_num, lg_avglatency));

	if (lg_avglatency > lg_maxavglatency) {
		pp_pl_log(0,
			  "%s: average latency (%lld us) is outside the thresold (%lld us)",
			  pp->dev, (long long)pow(base_num, lg_avglatency),
			  (long long)MAX_AVG_LATENCY);
		return DEFAULT_PRIORITY;
	}

	standard_deviation = calc_standard_deviation(lg_path_latency,
			index, lg_avglatency);
	/*
	 * In calPrio(), we let prio y = f(x) = log(max, base) - log (x, base);
	 * So if we want to let the priority of the latency outside 2 standard
	 * deviations can be distinguished from the latency inside 2 standard
	 * deviation, in others words at most 95% are the same and at least 5%
	 * are different according interval estimation of normal distribution,
	 * we should warn the user to set the base_num to be smaller if the
	 * log(x_threshold, base) is small than 2 standard deviation.
	 * x_threshold is derived from:
	 * y + 1 = f(x) + 1 = f(x) + log(base, base), so x_threadshold =
	 * base_num; Note that we only can compare the logarithm of x_threshold
	 * with the standard deviation because the standard deviation is derived
	 * from logarithm of latency.
	 *
	 * therefore , we recommend the base_num to meet the condition :
	 * 1 <= 2 * standard_deviation
	 */
	pp_pl_log(5, "%s: standard deviation for logarithm of latency = %.6f",
			pp->dev, standard_deviation);
	if (standard_deviation <= 0.5)
		pp_pl_log(3, "%s: the base_num(%.3lf) is too big to distinguish different priority "
			  "of two far-away latency. It is recommend to be set smaller",
			  pp->dev, base_num);
	/*
	 * If the standard deviation is too large , we should also warn the user
	 */

	if (standard_deviation > 4)
		pp_pl_log(3, "%s: the base_num(%.3lf) is too small to avoid noise disturbance "
			  ".It is recommend to be set larger",
			  pp->dev, base_num);


	rc = calcPrio(lg_avglatency, lg_maxavglatency, lg_minavglatency);

	return rc;
}
