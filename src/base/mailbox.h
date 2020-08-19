/*
 * Copyright (C) lichuang
 */

#pragma once

#include <atomic>
#include "base/define.h"
#include "base/lockfree_queue.h"
#include "base/typedef.h"
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
  bool  Send(IMessage *);
  void  Recv(IMessage **);

private:
  // Signaler to pass signals from writer thread to reader thread.
  Signaler signaler_;

  // True if there is unread data,is so, writer thread has to wakeup reader thread
  std::atomic_flag has_unread_data_;

  // True if there is thread writing data
  std::atomic_bool writing_;

  // one for multi writer threads,one for single reader thread
  LockFreeQueue<IMessage*> queue_[2];

  // current writer queue index
  std::atomic_short writer_index_;

  DISALLOW_COPY_AND_ASSIGN(Mailbox);
};

};