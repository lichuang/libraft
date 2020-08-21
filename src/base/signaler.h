/*
 * Copyright (C) lichuang
 */

#pragma once

#include "base/define.h"
#include "base/typedef.h"

namespace libraft {

class Signaler {
public:
  Signaler();
  ~Signaler();

  fd_t Fd() const {
    return rfd_;
  }
  void Send();
  int WaitGroup(int timeout);
  ssize_t Recv();

  ssize_t RecvFailable();
private:
  fd_t wfd_;
  fd_t rfd_;

  DISALLOW_COPY_AND_ASSIGN(Signaler);
};

};