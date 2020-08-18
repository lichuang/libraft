/*
 * Copyright (C) lichuang
 */

#include <errno.h>
#include <poll.h>
#include <unistd.h>
#include <sys/eventfd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "base/error.h"
#include "base/signaler.h"

namespace libraft {

static int
MakeFdPair(int *w, int *r) {
  int fd = eventfd(0, EFD_NONBLOCK);
  if (fd == -1) {
    *w = *r = -1;
    return kError;
  }

  *w = *r = fd;
  return kOK;
}

Signaler::Signaler()
  : wfd_(-1),
    rfd_(-1) {
  MakeFdPair(&wfd_, &rfd_);      
}

Signaler::~Signaler() {
  if (wfd_ != -1) {
    ::close(wfd_);
  }
  if (rfd_ != -1) {
    ::close(rfd_);
  }
}

void
Signaler::Send() {
  uint64_t dummy;
  while (true) {
    ssize_t nbytes = ::write(wfd_, &dummy, sizeof(dummy));
    if (nbytes == -1 && errno == EINTR) {
      continue;
    }
    break;
  }
}

int
Signaler::Wait(int timeout) {
  struct pollfd pfd;
  pfd.fd = rfd_;
  pfd.events = POLLIN;
  return ::poll(&pfd, 1, timeout);
}

ssize_t
Signaler::Recv() {
  uint64_t dummy;
  return ::read(rfd_, &dummy, sizeof(dummy));
}

ssize_t
Signaler::RecvFailable() {
  uint64_t dummy;
  return ::read(rfd_, &dummy, sizeof(dummy));
}

};
