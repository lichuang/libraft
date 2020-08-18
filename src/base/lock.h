/*
 * Copyright (C) lichuang
 */

#pragma once

#include <pthread.h>

namespace libraft {

class RWLock {
public :
  RWLock();
  ~RWLock();

  bool LockRead();
  bool LockWrite();
  bool UnLock();
private:
  pthread_rwlock_t rw_lock_;
};

class ReadLockGuard {
public:
  ReadLockGuard (RWLock& lock) : rw_lock_(lock) {
    rw_lock_.LockRead();
  }
  ~ReadLockGuard () {
    rw_lock_.UnLock();
  }
private:
  RWLock& rw_lock_;
};

class SafeWriteLockGuard {
public:
  SafeWriteLockGuard (RWLock &lock) : rw_lock_(lock) {
    rw_lock_.LockWrite();
  }
  ~SafeWriteLockGuard() {
    rw_lock_.UnLock();
  }
private:
  RWLock& rw_lock_;
};

template<class Lock>
class Guard {
public:  
  Guard(Lock& lock) : lock_(lock) {
    lock_.Lock();
  }

  ~Guard() {
    lock_.UnLock();
  }
private:
  Lock& lock_;
};

};