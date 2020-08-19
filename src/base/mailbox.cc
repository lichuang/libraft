/*
 * Copyright (C) lichuang
 */

#include "base/error.h"
#include "base/mailbox.h"
#include "base/message.h"

namespace libraft {

Mailbox::Mailbox()
  : has_unread_data_(ATOMIC_FLAG_INIT) {
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
  if (!has_unread_data_.test_and_set(std::memory_order_acquire)) {
    signaler_.Send();
  }

  return true;
}

void
Mailbox::Recv(IMessage** msg) {
  // switch the reader and writer queue
  short writer_idx = writer_index_.load(memory_order_acquire);
  writer_index_.store(1 - writer_idx, memory_order_acquire);  

  // clear the has_unread_data_ flag, so if there is new written data, 
  // writer thread will wake up reader thread
  has_unread_data_.clear();

  // wait for writer thread end operation
  while (writing_.load(memory_order_acquire)) {
  }

  IMessage *ret, *next;

  // old writer queue now is the reader queue
  LockFreeQueue<IMessage*> *queue = &(queue_[writer_idx]);

  // cause there is only one read thread,so no need thread-safe pop operation
  queue->UnsafePop(ret);
  // save message queue head
  *msg = ret;

  while (queue->UnsafePop(next)) {
    ret->Next(next);
    ret = next;
  }
}

};