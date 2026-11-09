/*
 * Copyright © Weaselway contributors
 * SPDX-License-Identifier: MIT
 */

#ifndef WSL_EVENTFD_H
#define WSL_EVENTFD_H

#include <fcntl.h>
#include <sys/eventfd.h>
#include <unistd.h>

/* An eventfd to hand to D3D12 as an event HANDLE. D3D12 on WSL reads
 * HANDLE 0 as NULL: it waits inline and never signals the fd. So an eventfd
 * that lands on fd 0 (stdin closed) is moved to a higher fd. -1 on error.
 */
static inline int
wsl_eventfd(int flags)
{
   int fd = eventfd(0, flags);
   if (fd != 0)
      return fd;

   int moved = fcntl(fd, (flags & EFD_CLOEXEC) ? F_DUPFD_CLOEXEC : F_DUPFD, 1);
   close(fd);
   return moved;
}

#endif
