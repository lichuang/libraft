/*
 * Copyright (C) lichuang
 */

#include <sys/prctl.h>
#include "base/thread.h"
#include "base/spin_lock.h"

namespace libraft {

// per thread info
struct perThreadInfo {
  Thread *thread;
  string name;

  perThreadInfo()
    : thread(NULL),
      name("main") {      
  }

  ~perThreadInfo() {
  }
};

thread_local static perThreadInfo gPerThreadInfo;

// thread entry information
struct threadEntry {
  SpinLock* lock;
  Thread* thread;
};

Thread::Thread(const string& name)
  : tid_(0),
    name_(name),
    state_(kThreadNone),
    callback_(NULL) {
}

Thread::Thread(const string& name, ThreadCallback func)
  : tid_(0),
    name_(name),
    state_(kThreadNone),
    callback_(func) {
}

Thread::~Thread() {
}

int
Thread::Start() {
  SpinLock lock;

  // Use a spinlock to wait for thread to finish intialization
  lock.Lock();
  threadEntry entry = {.lock = &lock, .thread = this};
  int ret = pthread_create(&tid_, NULL, Thread::main, &entry);

  // Double-lock. This blocks until the thread unlocks spinlock
  lock.Lock();

  return ret;
}

void
Thread::Join() {
  state_ = kThreadStopped;

  if (tid_ != 0) {
    pthread_join(tid_, NULL);
    tid_ = 0;
  }
}

void*
Thread::main(void* arg) {
  threadEntry *entry = static_cast<threadEntry*>(arg);
  SpinLock *lock = entry->lock;
  Thread* thread = entry->thread;

  ::prctl(PR_SET_NAME, thread->name_.c_str());
  gPerThreadInfo.thread = thread;
  gPerThreadInfo.name = thread->name_;

  thread->state_ = kThreadRunning;

  // Unblock the main thread
  lock->UnLock();

  if (thread->callback_ == NULL) {
    thread->Run();
  } else {
    thread->callback_();
  }

  return NULL;
}

const string&
CurrentThreadName() {
  return gPerThreadInfo.name;
}

};
