/*
 * Copyright © 2012 Collabora, Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "config.h"

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>

#ifdef HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif
#ifdef __FreeBSD__
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <libprocstat.h>
#endif

#include "test-runner.h"

#if defined(__FreeBSD__)
/*
 * On FreeBSD, we print file descriptor details using libprocstat since that
 * does not depend on fdescfs (which provides /dev/fd/N) being mounted.
 */
static int
count_open_fds_sysctl(void)
{
	int error;
	int nfds;
	int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_NFDS, 0 };
	size_t len;

	len = sizeof(nfds);
	error = sysctl(mib, 4, &nfds, &len, NULL, 0);
	assert(error == 0 && "sysctl KERN_PROC_NFDS failed.");
	return nfds;
}

static int
list_open_fds_libprocstat(void)
{
	unsigned int nprocs = 0;
	unsigned int nfds = 0;
	int flags;
	struct procstat *prstat;
	struct kinfo_proc *kp;
	struct filestat *fst;
	struct filestat_list *prfiles;

	prstat = procstat_open_sysctl();
	assert(prstat != NULL && "Failed to init libprocstat");
	kp = procstat_getprocs(prstat, KERN_PROC_PID, getpid(), &nprocs);
	assert(kp != NULL);
	assert(nprocs == 1);
	prfiles = procstat_getfiles(prstat, kp, 0);
	assert(prfiles != NULL);
	STAILQ_FOREACH(fst, prfiles, next) {
		if (fst->fs_fd == -1)
			continue;
		flags = fcntl(fst->fs_fd, F_GETFD);
		fprintf(stderr, "fd[%d]=%d, type=%d, path=%s, flags=%#x%s\n",
			nfds, fst->fs_fd, fst->fs_type, fst->fs_path, flags,
			(flags & FD_CLOEXEC) ? " (includes FD_CLOEXEC)" : "");
		nfds++;
	}
	procstat_freeprocs(prstat, kp);
	procstat_freefiles(prstat, prfiles);
	procstat_close(prstat);
	return nfds;
}

static int
count_open_fds_impl(bool print_descriptors)
{
	int count;

	if (print_descriptors) {
		count = list_open_fds_libprocstat();
		assert(count == count_open_fds_sysctl());
	} else {
		count = count_open_fds_sysctl();
	}
	return count;
}
#else

static int
count_open_fds_impl(bool print_descriptors)
{
	int count = 0;
	DIR *dir;
	struct dirent *ent;
	int opendirfd;
	int curfd;
	bool found_opendirfd = false;

	/*
	 * Using /dev/fd instead of /proc/self/fd should allow this code to
	 * work on non-Linux operating systems.
	 */
	dir = opendir("/dev/fd");
	assert(dir && "opening /dev/fd failed.");
	opendirfd = dirfd(dir);
	assert(opendirfd >= 0);

	errno = 0;
	while ((ent = readdir(dir))) {
		const char *s = ent->d_name;
		if (s[0] == '.' && (s[1] == 0 || (s[1] == '.' && s[2] == 0)))
			continue;
		errno = 0;
		curfd = strtol(ent->d_name, NULL, 10);
		if (errno != 0) {
			fprintf(stderr, "Unexpected file name '%s'\n",
				ent->d_name);
			abort();
		}
		if (curfd == opendirfd) {
			/* Don't count the file descriptor we just opened. */
			found_opendirfd = true;
			continue;
		}
		if (print_descriptors) {
			int flags;
			char path[PATH_MAX];
			ssize_t namelen;

			flags = fcntl(curfd, F_GETFD);
			path[0] = '\0';
			namelen = readlinkat(opendirfd, ent->d_name, path, sizeof(path));
			assert(namelen >= 0 && "readlinkat failed");
			path[namelen] = '\0';
			fprintf(stderr, "fd[%d]=%d, path=%s, flags=%#x%s\n",
				count, curfd, path, flags,
				(flags & FD_CLOEXEC) ? " (includes FD_CLOEXEC)"
						     : "");
		}
		count++;
	}
	assert(errno == 0 && "reading /dev/fd failed.");
	assert(found_opendirfd && "Did not see fd from opendir().");

	closedir(dir);

	return count;
}
#endif

int
list_open_fds(void)
{
	return count_open_fds_impl(true);
}

int
count_open_fds(void)
{
	return count_open_fds_impl(false);
}

void
exec_fd_leak_check(int nr_expected_fds)
{
	const char *exe = "exec-fd-leak-checker";
	char number[16] = { 0 };
	const char *test_build_dir = getenv("TEST_BUILD_DIR");
	char exe_path[256] = { 0 };

	if (getenv("TEST_DEBUG_FD_LEAK_CHECK")) {
		fprintf(stderr, "Calling %s(%d)\n", __func__, nr_expected_fds);
		fprintf(stderr, "FDs before exec\n");
		list_open_fds();
	}

	if (test_build_dir == NULL || test_build_dir[0] == 0) {
	        test_build_dir = ".";
	}

	snprintf(exe_path, sizeof exe_path - 1, "%s/%s", test_build_dir, exe);

	snprintf(number, sizeof number - 1, "%d", nr_expected_fds);
	execl(exe_path, exe, number, (char *)NULL);
	fprintf(stderr, "Failed to execute '%s %s'\n", exe_path, number);
	abort();
}

#define USEC_TO_NSEC(n) (1000 * (n))

/* our implementation of usleep and sleep functions that are safe to use with
 * timeouts (timeouts are implemented using alarm(), so it is not safe use
 * usleep and sleep. See man pages of these functions)
 */
void
test_usleep(useconds_t usec)
{
	struct timespec ts = {
		.tv_sec = 0,
		.tv_nsec = USEC_TO_NSEC(usec)
	};

	assert(nanosleep(&ts, NULL) == 0);
}

/* we must write the whole function instead of
 * wrapping test_usleep, because useconds_t may not
 * be able to contain such a big number of microseconds */
void
test_sleep(unsigned int sec)
{
	struct timespec ts = {
		.tv_sec = sec,
		.tv_nsec = 0
	};

	assert(nanosleep(&ts, NULL) == 0);
}

/** Try to disable coredumps
 *
 * Useful for tests that crash on purpose, to avoid creating a core file
 * or launching an application crash handler service or cluttering coredumpctl.
 *
 * NOTE: Calling this may make the process undebuggable.
 */
void
test_disable_coredumps(void)
{
	struct rlimit r;

	if (getrlimit(RLIMIT_CORE, &r) == 0) {
		r.rlim_cur = 0;
		setrlimit(RLIMIT_CORE, &r);
	}

#if defined(HAVE_PRCTL) && defined(PR_SET_DUMPABLE)
	prctl(PR_SET_DUMPABLE, 0, 0, 0, 0);
#endif
}
