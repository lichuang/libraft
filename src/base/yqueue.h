/*
 * Copyright (C) lichuang
 */

#pragma once

#include <stdlib.h>
#include <stddef.h>

#include "base/atomic.h"
#include "base/define.h"

BEGIN_NAMESPACE

//  yqueue is an efficient queue implementation. The main goal is
//  to minimise number of allocations/deallocations needed. Thus yqueue
//  allocates/deallocates elements in batches of N.
//
//  yqueue allows one thread to use push/back function and another one
//  to use pop/front functions. However, user must ensure that there's no
//  pop on the empty queue and that both threads don't access the same
//  element in unsynchronised manner.
//
//  T is the type of the object in the queue.
//  N is granularity of the queue (how many pushes have to be done till
//  actual memory allocation is required).
#ifdef HAVE_POSIX_MEMALIGN
// ALIGN is the memory alignment size to use in the case where we have
// posix_memalign available. Default value is 64, this alignment will
// prevent two queue chunks from occupying the same CPU cache line on
// architectures where cache lines are <= 64 bytes (e.g. most things
// except POWER).
template <typename T, int N, size_t ALIGN = 64> class YQueue
#else
template <typename T, int N> class YQueue
#endif
{
  public:
    //  Create the queue.
    inline YQueue() {
        begin_chunk_ = allocate_chunk();
        begin_pos_ = 0;
        back_chunk_ = NULL;
        back_pos_ = 0;
        end_chunk_ = begin_chunk_;
        end_pos_ = 0;
    }

    //  Destroy the queue.
    inline ~YQueue() {
        while (true) {
            if (begin_chunk_ == end_chunk_) {
                free(begin_chunk_);
                break;
            }
            chunk_t *o = begin_chunk_;
            begin_chunk_ = begin_chunk_->next;
            free(o);
        }

        chunk_t *sc = spare_chunk_.Xchg(NULL);
        free(sc);
    }

    //  Returns reference to the front element of the queue.
    //  If the queue is empty, behaviour is undefined.
    inline T &Front() { 
        return begin_chunk_->values[begin_pos_];
    }

    //  Returns reference to the back element of the queue.
    //  If the queue is empty, behaviour is undefined.
    inline T &Back() { 
        return back_chunk_->values[back_pos_]; 
    }

    //  Adds an element to the back end of the queue.
    inline void Push() {
        back_chunk_ = end_chunk_;
        back_pos_ = end_pos_;

        if (++end_pos_ != N)
            return;

        chunk_t *sc = spare_chunk_.Xchg(NULL);
        if (sc) {
            end_chunk_->next = sc;
            sc->prev = end_chunk_;
        } else {
            end_chunk_->next = allocate_chunk();
            end_chunk_->next->prev = end_chunk_;
        }
        end_chunk_ = end_chunk_->next;
        end_pos_ = 0;
    }

    //  Removes element from the back end of the queue. In other words
    //  it rollbacks last push to the queue. Take care: Caller is
    //  responsible for destroying the object being unpushed.
    //  The caller must also guarantee that the queue isn't empty when
    //  unpush is called. It cannot be done automatically as the read
    //  side of the queue can be managed by different, completely
    //  unsynchronised thread.
    inline void UnPush() {
        //  First, move 'back' one position backwards.
        if (back_pos_)
            --back_pos_;
        else {
            back_pos_ = N - 1;
            back_chunk_ = back_chunk_->prev;
        }

        //  Now, move 'end' position backwards. Note that obsolete end chunk
        //  is not used as a spare chunk. The analysis shows that doing so
        //  would require free and atomic operation per chunk deallocated
        //  instead of a simple free.
        if (end_pos_)
            --end_pos_;
        else {
            end_pos_ = N - 1;
            end_chunk_ = end_chunk_->prev;
            free (end_chunk_->next);
            end_chunk_->next = NULL;
        }
    }

    //  Removes an element from the front end of the queue.
    inline void Pop() {
        if (++begin_pos_ == N) {
            chunk_t *o = begin_chunk_;
            begin_chunk_ = begin_chunk_->next;
            begin_chunk_->prev = NULL;
            begin_pos_ = 0;

            //  'o' has been more recently used than spare_chunk_,
            //  so for cache reasons we'll get rid of the spare and
            //  use 'o' as the spare.
            chunk_t *cs = spare_chunk_.Xchg(o);
            free(cs);
        }
    }

  private:
    //  Individual memory chunk to hold N elements.
    struct chunk_t {
        T values[N];
        chunk_t *prev;
        chunk_t *next;
    };

    inline chunk_t *allocate_chunk () {
#ifdef HAVE_POSIX_MEMALIGN
        void *pv;
        if (posix_memalign(&pv, ALIGN, sizeof (chunk_t)) == 0)
            return (chunk_t *) pv;
        return NULL;
#else
        return static_cast<chunk_t *>(malloc(sizeof(chunk_t)));
#endif
    }

    //  Back position may point to invalid memory if the queue is empty,
    //  while begin & end positions are always valid. Begin position is
    //  accessed exclusively be queue reader (front/pop), while back and
    //  end positions are accessed exclusively by queue writer (back/push).
    chunk_t *begin_chunk_;
    int begin_pos_;
    chunk_t *back_chunk_;
    int back_pos_;
    chunk_t *end_chunk_;
    int end_pos_;

    //  People are likely to produce and consume at similar rates.  In
    //  this scenario holding onto the most recently freed chunk saves
    //  us from having to call malloc/free.
    AtomicPointer<chunk_t> spare_chunk_;

    //  Disable copying of yqueue.
    DISALLOW_COPY_AND_ASSIGN(YQueue);
};

END_NAMESPACE