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
class Worker;

// worker thread message mailbox
class Mailbox {
public:
  Mailbox(Worker*);
  ~Mailbox();

  fd_t Fd() const {
    return signaler_.Fd();
  }
  bool  Send(IMessage *);
  void  Recv();

private:
  // Signaler to pass signals from writer thread to reader thread.
  Signaler signaler_;

  // True if there is unread data,is so, writer thread has to wakeup reader thread
  std::atomic_flag notified_;

  // True if there is thread writing data
  std::atomic_bool writing_;

  // one for multi writer threads,one for single reader thread
  LockFreeQueue<IMessage*> queue_[2];

  // current writer queue index
  std::atomic_short writer_index_;

  Worker *worker_;
  DISALLOW_COPY_AND_ASSIGN(Mailbox);
};

};