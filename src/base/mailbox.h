/*
 * Copyright (C) lichuang
 */

#pragma once

#include "base/define.h"
#include "base/typedef.h"
#include "base/ypipe.h"
#include "base/mutex.h"
#include "base/signaler.h"

namespace libraft {

class IMessage;

// worker message mailbox
class Mailbox {
public:
  Mailbox();
  ~Mailbox();

  fd_t Fd() const {
    return signaler_.Fd();
  }
  void Send(IMessage *);
  int  Recv(IMessage **, int timeout);

private:
  // The pipe to store actual commands.
  typedef YPipe<IMessage*, 16> cpipe_t;
  cpipe_t pipe_;

  // Signaler to pass signals from writer thread to reader thread.
  Signaler signaler_;

  //  There's only one thread receiving from the mailbox, but there
  //  is arbitrary number of threads sending. Given that ypipe requires
  //  synchronised access on both of its endpoints, we have to synchronise
  //  the sending side.
  Mutex sync_;

  //  True if the underlying pipe is active, ie. when we are allowed to
  //  read commands from it.
  bool active_;

  DISALLOW_COPY_AND_ASSIGN(Mailbox);
};

};