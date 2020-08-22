/*
 * Copyright (C) lichuang
 */

#include "base/status.h"
#include "base/mailbox.h"
#include "base/message.h"
#include "base/worker.h"

namespace libraft {

Mailbox::Mailbox(Worker* worker)
  : notified_(ATOMIC_FLAG_INIT),
    worker_(worker) {
#ifdef USE_STL_CONTAINER  
  writer_index_ = 0;
#else 
  writer_index_.store(0, memory_order_relaxed);
  writing_.store(false, memory_order_relaxed);
#endif
}

Mailbox::~Mailbox() {
}

bool 
Mailbox::Send(IMessage *msg) {
#ifdef USE_STL_CONTAINER
  std::lock_guard<std::mutex> lock(mutex_);
  queue_[writer_index_].emplace_back(msg);
  // if reader thread is sleeping, send a signal to wake up the reader
  if (!notified_.load()) {
    notified_.store(true);
    worker_->notify();
  }  
#else  
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
#endif

  return true;
}

void
Mailbox::Recv() {
#ifdef USE_STL_CONTAINER
  MsgQueue* read_queue;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    read_queue = &queue_[writer_index_];
    writer_index_ = 1 - writer_index_;
    notified_.store(false);
  }
  MsgQueue::iterator iter = read_queue->begin();
  while (iter != read_queue->end()) {
    IMessage* msg = *iter;
    worker_->process(msg);
    ++iter;
  }
  read_queue->clear();
#else  
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
#endif  
}

};