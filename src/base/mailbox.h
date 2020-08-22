/*
 * Copyright (C) lichuang
 */

#pragma once

#include <atomic>
#include "base/define.h"
#include "base/lockfree_queue.h"
#include "base/typedef.h"

// if define USE_STL_CONTAINER,use STL container implement message queue
#define USE_STL_CONTAINER

#ifdef USE_STL_CONTAINER
#include <list>
#include <mutex>
#endif

using namespace std;

namespace libraft {

class IMessage;
class Worker;

// worker thread message mailbox
class Mailbox {
public:
  Mailbox(Worker*);
  ~Mailbox();

  bool  Send(IMessage *);
  void  Recv();

private:
  std::atomic<bool> notified_;

#ifdef USE_STL_CONTAINER
  std::mutex mutex_;
  typedef list<IMessage*> MsgQueue;
  MsgQueue queue_[2];
  int writer_index_;
#else
  // True if there is thread writing data
  std::atomic_bool writing_;

  // one for multi writer threads,one for single reader thread
  LockFreeQueue<IMessage*> queue_[2];

  // current writer queue index
  std::atomic_short writer_index_;
#endif

  Worker *worker_;
  DISALLOW_COPY_AND_ASSIGN(Mailbox);
};

};