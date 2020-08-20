/*
 * Copyright (C) lichuang
 */

#include "base/error.h"
#include "base/mailbox.h"
#include "base/message.h"
#include "base/worker.h"

namespace libraft {

Mailbox::Mailbox(Worker* worker)
  : notified_(ATOMIC_FLAG_INIT),
    worker_(worker) {
  writer_index_.store(0, memory_order_relaxed);
  writing_.store(false, memory_order_relaxed);
}

Mailbox::~Mailbox() {
}

bool 
Mailbox::Send(IMessage *msg) {
  writing_.store(true, memory_order_acquire);

  short idx = writer_index_.load(memory_order_acquire);
  bool ok = queue_[idx].Push(msg);
  writing_.store(false, memory_order_relaxed);

  if (!ok) {
    return false;
  }
  // if reader thread is sleeping, send a signal to wake up the reader
  if (!notified_.test_and_set(std::memory_order_acquire)) {
    worker_->notify();
  }

  return true;
}

void
Mailbox::Recv() {
  // switch the reader and writer queue
  short writer_idx = writer_index_.load(memory_order_acquire);
  writer_index_.store(1 - writer_idx, memory_order_acquire);  

  // clear the notified_ flag, so if there is new written data, 
  // writer thread will wake up reader thread
  notified_.clear();

  // wait for writer thread end operation
  while (writing_.load(memory_order_acquire)) {
  }

  IMessage *msg;
  // old writer queue now is the reader queue
  LockFreeQueue<IMessage*> *queue = &(queue_[writer_idx]);

  // cause there is only one reader thread,so no need thread-safe pop operation
  while (queue->UnsafePop(msg)) {
    worker_->process(msg);
  }
}

};