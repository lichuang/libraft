/*
 * Copyright (C) lichuang
 */

#pragma once

#include <atomic>
#include "base/lock.h"
#include "base/likely.h"

namespace libraft {

class SpinLock {
public:
  SpinLock() {
  }

  ~ SpinLock() {
  }

  inline void Lock() {
    // first try lock quickly
    if (likely(!lock_.exchange(true, std::memory_order_acquire)))
      return;
    // fail, then try slower one...
    lockSlow();
  }

  inline void UnLock() {
    // release lock
    lock_.store(false, std::memory_order_release);
  }

private:
  // This is called if the initial attempt to acquire the lock fails. It's
  // slower, but has a much better scheduling and power consumption behavior.
  void lockSlow();

private:
  std::atomic_int lock_{0};
};

typedef Guard<SpinLock> SpinLockGuard;

};