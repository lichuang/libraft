/*
 * Copyright (C) lichuang
 */

#pragma once

#include <atomic>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include "base/define.h"

namespace libraft {

//  This class encapsulates several atomic operations on pointers.
template <typename T>
class AtomicPointer {
public:
  //  Initialise atomic pointer
  inline AtomicPointer() : ptr_(NULL) { }

  //  Set value of atomic pointer
  //  Use this function only when you are sure that at most one
  //  thread is accessing the pointer at the moment.
  inline void Set(T *ptr) { ptr_.store(ptr); }

  //  Perform atomic 'exchange pointers' operation. Pointer is set
  //  to the 'val_' value. Old value is returned.
  inline T *Xchg (T *val) {
    //return static_cast<T *>(atomic_xchg_ptr (&_ptr, val_));
    return ptr_.exchange(val, std::memory_order_acq_rel);
  }

  //  Perform atomic 'compare and swap' operation on the pointer.
  //  The pointer is compared to 'cmp' argument and if they are
  //  equal, its value is set to 'val_'. Old value of the pointer
  //  is returned.
  inline T *Cas(T *cmp, T *val) {
    ptr_.compare_exchange_strong(cmp, val, std::memory_order_acq_rel);
    return cmp;
  }

private:
  std::atomic<T *> ptr_;

  DISALLOW_COPY_AND_ASSIGN(AtomicPointer);
};

class AtomicCounter {
public:
  typedef uint32_t integer_t;

  inline AtomicCounter(integer_t value = 0)
    : value_(value) {
  }

  //  Set counter value_
  inline void Set(integer_t value) { value_.store(value); }

  //  Atomic addition. Returns the old value_.
  inline integer_t Add(integer_t increment) {
    integer_t old_value;
    old_value = value_.fetch_add(increment, std::memory_order_acq_rel);

    return old_value;
  }

  //  Atomic subtraction. Returns false if the counter drops to zero.
  inline bool Sub(integer_t decrement) {
    integer_t old = value_.fetch_sub(decrement, std::memory_order_acq_rel);
    return old - decrement != 0;
  }

  inline integer_t Get() const { return value_; }

private:
  std::atomic<integer_t> value_;

  DISALLOW_COPY_AND_ASSIGN(AtomicCounter);
} __attribute__ ((aligned (sizeof (void *))));

};