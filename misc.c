/*
 Copyright 2008 Google Inc.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#define _FILE_OFFSET_BITS 64
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "commands.h"
#define __USE_GNU
#include <sched.h>

static int
busy_loop(int argc, const char *argv[], const struct cmd_info *info)
{
	unsigned long long count = 0;
	struct timeval t0;
	struct timeval t1;
	int reps;
	int printed = 0;

	if (argc == 2) {
		reps = atoi(argv[1]);
	} else {
		reps = -1;
	}

	gettimeofday(&t0, NULL);
	while (printed != reps) {
		count++;
		if ((count % 16384) == 0) {
			unsigned int elapsed;
			gettimeofday(&t1, NULL);
			elapsed = (t1.tv_sec - t0.tv_sec)*1000000;
			elapsed += t1.tv_usec - t0.tv_usec;
			if (elapsed >= 1000000) {
				printf("%llu\n", count);
				gettimeofday(&t0, NULL);
				count = 0;
				if (reps != -1) {
					printed++;
				}
			}
		}
	}

	return EXIT_SUCCESS;
}

#define rdtscll(val) do { \
	uint32_t __a, __d; \
	__asm__ __volatile__("rdtsc" : "=a" (__a), "=d" (__d)); \
	(val) = ((uint64_t)__a) | (((uint64_t)__d)<<32); \
} while(0)

static int
rdtsc(int argc, const char *argv[], const struct cmd_info *info)
{
	unsigned long long tsc;

	rdtscll(tsc);
	printf("0x%016llx\n", tsc);

	return EXIT_SUCCESS;
}

static int
cpuid(int argc, const char *argv[], const struct cmd_info *info)
{
	off_t function;
	off_t index;
	off_t offset;
	int cpu;
	uint32_t data[4];
	char dev[512];
	int fd;

	cpu = strtol(argv[1], NULL, 0);
	function = strtoul(argv[2], NULL, 0);
	index = 0;
	if (argc == 4) {
		index = strtoul(argv[3], NULL, 0);
	}
	offset = (index << 32) | function;

	snprintf(dev, sizeof(dev), "/dev/cpu/%d/cpuid", cpu);
	fd = open(dev, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "open(\"%s\"): %s\n", dev, strerror(errno));
		return EXIT_FAILURE;
	}

	if (lseek(fd, offset, SEEK_SET) == (off_t)-1) {
		fprintf(stderr, "lseek(%llu): %s\n",
		        (unsigned long long)offset, strerror(errno));
		close(fd);
		return EXIT_FAILURE;
	}

	if (read(fd, &data, sizeof(data)) != sizeof(data)) {
		fprintf(stderr, "read(): %s\n", strerror(errno));
		close(fd);
		return EXIT_FAILURE;
	}
	close(fd);

	printf("0x%08x 0x%08x 0x%08x 0x%08x\n",
	       data[0], data[1], data[2], data[3]);

	return EXIT_SUCCESS;
}

/*
 * runon()
 *
 * At least two parameters :
 *
 * argv[1]: cpu to run command on
 * argv[2]: command to run
 * argv[3]  and on are arguments to pass to exec()
 *
 */
static int
runon(int argc, const char *argv[], const struct cmd_info *info)
{
	unsigned long cpu;
	cpu_set_t cpuset;

	/* strip program command line */
	argc--; argv++;

	/* if we wanted to be clever we could parse it as cpu,cpu,cpu */
	cpu = strtoul(argv[0], NULL, 0);
	argc--; argv++;

	/* run on the specified CPU */
	CPU_ZERO(&cpuset);
	CPU_SET(cpu, &cpuset);
	if (sched_setaffinity(getpid(), sizeof(cpuset), &cpuset) < 0) {
		perror("sched_setaffinity()");
		return EXIT_FAILURE;
	}

	/* load the target */
	execvp(argv[0], (void *)argv);

	/* if we get here, we didn't exec... */
	perror("exec");

	return EXIT_FAILURE;
}

MAKE_PREREQ_PARAMS_VAR_ARGS(cpuid_params, 3, 4, "<cpu> <function> [index]", 0);
MAKE_PREREQ_PARAMS_VAR_ARGS(runon_params, 3, INT_MAX, "<cpu> <cmd> [args]", 0);

static const struct cmd_info misc_cmds[] = {
	MAKE_CMD(rdtsc, rdtsc, NULL),
	MAKE_CMD(busy_loop, &busy_loop, NULL),
	MAKE_CMD_WITH_PARAMS(cpuid, cpuid, NULL, &cpuid_params),
	MAKE_CMD_WITH_PARAMS(runon, &runon, NULL, &runon_params),
};

MAKE_CMD_GROUP(MISC, NULL, misc_cmds);
REGISTER_CMD_GROUP(MISC);