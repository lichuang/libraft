/*
 * Copyright (C) lichuang
 */

#include <sched.h>
#include <unistd.h>
#include "base/spin_lock.h"

#define YIELD_PROCESSOR __asm__ __volatile__("pause")
#define YIELD_THREAD sched_yield()

namespace libraft {

void
SpinLock::lockSlow() {
  // The value of |kYieldProcessorTries| is cargo culted from TCMalloc, Windows
  // critical section defaults, and various other recommendations.
  // TODO(jschuh): Further tuning may be warranted.
  static const int kYieldProcessorTries = 1000;
  // The value of |kYieldThreadTries| is completely made up.
  static const int kYieldThreadTries = 10;
  int yield_thread_count = 0;
  do {
    do {
      for (int count = 0; count < kYieldProcessorTries; ++count) {
        // Let the processor know we're spinning.
        YIELD_PROCESSOR;
        if (!lock_.load(std::memory_order_relaxed) &&
            likely(!lock_.exchange(true, std::memory_order_acquire)))
          return;
      }

      if (yield_thread_count < kYieldThreadTries) {
        ++yield_thread_count;
        // Give the OS a chance to schedule something on this core.
        YIELD_THREAD;
      } else {
        // At this point, it's likely that the lock is held by a lower priority
        // thread that is unavailable to finish its work because of higher
        // priority threads spinning here. Sleeping should ensure that they make
        // progress.
        ::usleep(1000);;
      }
    } while (lock_.load(std::memory_order_relaxed));
  } while (unlikely(lock_.exchange(true, std::memory_order_acquire)));
}

};