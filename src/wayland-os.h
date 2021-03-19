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

#ifndef WAYLAND_OS_H
#define WAYLAND_OS_H

#ifdef __FreeBSD__
#include <sys/param.h> /* To get __FreeBSD_version. */
#if __FreeBSD_version < 1300502 || \
    (__FreeBSD_version >= 1400000 && __FreeBSD_version < 1400006)
/*
 * FreeBSD had a broken implementation of MSG_CMSG_CLOEXEC between 2015 and
 * 2021. Check if we are compiling against a version that includes the fix
 * (https://cgit.freebsd.org/src/commit/?id=6ceacebdf52211).
 */
#define HAVE_BROKEN_MSG_CMSG_CLOEXEC
#endif
#endif

int
wl_os_socket_cloexec(int domain, int type, int protocol);

int
wl_os_dupfd_cloexec(int fd, int minfd);

ssize_t
wl_os_recvmsg_cloexec(int sockfd, struct msghdr *msg, int flags);

int
wl_os_epoll_create_cloexec(void);

int
wl_os_accept_cloexec(int sockfd, struct sockaddr *addr, socklen_t *addrlen);


/*
 * The following are for wayland-os.c and the unit tests.
 * Do not use them elsewhere.
 */

#ifdef __linux__

#ifndef SOCK_CLOEXEC
#define SOCK_CLOEXEC 02000000
#endif

#ifndef F_DUPFD_CLOEXEC
#define F_DUPFD_CLOEXEC 1030
#endif

#ifndef MSG_CMSG_CLOEXEC
#define MSG_CMSG_CLOEXEC 0x40000000
#endif

#endif /* __linux__ */

#endif
