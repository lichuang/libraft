/*
 * Copyright (C) lichuang
 */

#pragma once

#include "base/define.h"
#include "base/atomic.h"
#include "base/yqueue.h"

namespace libraft {

//  Lock-free queue implementation.
//  Only a single thread can read from the pipe at any specific moment.
//  Only a single thread can write to the pipe at any specific moment.
//  T is the type of the object in the queue.
//  N is granularity of the pipe, i.e. how many items are needed to
//  perform next memory allocation.
template <typename T, int N> 
class YPipe {
public:
  //  Initialises the pipe.
  inline YPipe () {
    //  Insert terminator element into the queue.
    queue_.Push ();

    //  Let all the pointers to point to the terminator.
    //  (unless pipe is dead, in which case c is set to NULL).
    r_ = w_ = f_ = &queue_.Back();
    c_.Set(&queue_.Back());
  }

  //  The destructor doesn't have to be virtual. It is made virtual
  //  just to keep ICC and code checking tools from complaining.
  inline virtual ~YPipe () {}

  //  Following function (write) deliberately copies uninitialised data
  //  when used with zmq_msg. Initialising the VSM body for
  //  non-VSM messages won't be good for performance.

  //  Write an item to the pipe.  Don't flush it yet. If incomplete is
  //  set to true the item is assumed to be continued by items
  //  subsequently written to the pipe. Incomplete items are never
  //  flushed down the stream.
  inline void Write(const T &value_, bool incomplete_) {
    //  Place the value to the queue, add new terminator element.
    queue_.Back() = value_;
    queue_.Push();

    //  Move the "flush up to here" poiter.
    if (!incomplete_)
      f_ = &queue_.Back();
  }

  //  Pop an incomplete item from the pipe. Returns true if such
  //  item exists, false otherwise.
  inline bool UnWrite(T *value_) {
    if (f_ == &queue_.Back())
      return false;
    queue_.UnPush();
    *value_ = queue_.Back();
    return true;
  }

  //  Flush all the completed items into the pipe. Returns false if
  //  the reader thread is sleeping. In that case, caller is obliged to
  //  wake the reader up before using the pipe again.
  inline bool Flush() {
    //  If there are no un-flushed items, do nothing.
    if (w_ == f_)
      return true;

    //  Try to set 'c' to 'f'.
    if (c_.Cas(w_, f_) != w_) {
      //  Compare-and-swap was unseccessful because 'c' is NULL.
      //  This means that the reader is asleep. Therefore we don't
      //  care about thread-safeness and update c in non-atomic
      //  manner. We'll return false to let the caller know
      //  that reader is sleeping.
      c_.Set(f_);
      w_ = f_;
      return false;
    }

    //  Reader is alive. Nothing special to do now. Just move
    //  the 'first un-flushed item' pointer to 'f'.
    w_ = f_;
    return true;
  }

  //  Check whether item is available for reading.
  inline bool CheckRead() {
    //  Was the value prefetched already? If so, return.
    if (&queue_.Front() != r_ && r_)
      return true;

    //  There's no prefetched value, so let us prefetch more values.
    //  Prefetching is to simply retrieve the
    //  pointer from c in atomic fashion. If there are no
    //  items to prefetch, set c to NULL (using compare-and-swap).
    r_ = c_.Cas(&queue_.Front(), NULL);

    //  If there are no elements prefetched, exit.
    //  During pipe's lifetime r should never be NULL, however,
    //  it can happen during pipe shutdown when items
    //  are being deallocated.
    if (&queue_.Front() == r_ || !r_)
      return false;

    //  There was at least one value prefetched.
    return true;
  }

  //  Reads an item from the pipe. Returns false if there is no value.
  //  available.
  inline bool Read(T *value_) {
    //  Try to prefetch a value.
    if (!CheckRead())
      return false;

    //  There was at least one value prefetched.
    //  Return it to the caller.
    *value_ = queue_.Front();
    queue_.Pop();
    return true;
  }

  //  Applies the function fn to the first elemenent in the pipe
  //  and returns the value returned by the fn.
  //  The pipe mustn't be empty or the function crashes.
  inline bool Probe(bool (*fn_) (const T &)) {
    bool rc = CheckRead();

    return (*fn_)(queue_.Front());
  }

protected:
  //  Allocation-efficient queue to store pipe items.
  //  Front of the queue points to the first prefetched item, back of
  //  the pipe points to last un-flushed item. Front is used only by
  //  reader thread, while back is used only by writer thread.
  YQueue<T, N> queue_;

  //  Points to the first un-flushed item. This variable is used
  //  exclusively by writer thread.
  T *w_;

  //  Points to the first un-prefetched item. This variable is used
  //  exclusively by reader thread.
  T *r_;

  //  Points to the first item to be flushed in the future.
  T *f_;

  //  The single point of contention between writer and reader thread.
  //  Points past the last flushed item. If it is NULL,
  //  reader is asleep. This pointer should be always accessed using
  //  atomic operations.
  AtomicPointer<T> c_;

  //  Disable copying of ypipe object.
  DISALLOW_COPY_AND_ASSIGN(YPipe);
};

};