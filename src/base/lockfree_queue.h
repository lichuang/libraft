/*
 * Copyright (C) lichuang
 */

#pragma once

#include <atomic>
#include "base/likely.h"

using namespace std;

namespace libraft {

template <typename T>
class LockFreeQueue {
private:
  typedef unsigned long tagged_node_t;
  static const int kLockFreeCacheLineBytes = 64;
  static const int kPaddingSize = kLockFreeCacheLineBytes - sizeof(tagged_node_t);
	
	struct node {
		T data;
		atomic_ulong next;

		node(T const &v) : data(v) {
			next.store((tagged_node_t)0, memory_order_release);
		}

		node() {}
	};

public:
	LockFreeQueue() {
		node *n = new node();
		head_.store((tagged_node_t)n, memory_order_relaxed);
		tail_.store((tagged_node_t)n, memory_order_release);
	}

	bool Empty() {
		return head_.load(memory_order_acquire) == tail_.load(memory_order_acquire);
	}

	bool Push(T const &t) {
		node *n = new node(t);

		if (!n) return false;
		
		for (;;) {			
			tagged_node_t tail = tail_.load(memory_order_acquire);

			node *tNext   = tag2nextNodePtr(tail_);
			node *tailPtr = tag2nodePtr(tail_);

			tagged_node_t tail2= tail_.load(memory_order_acquire);
			if (likely(tail == tail2)) {

				if (tNext == 0) {
					tagged_node_t next = 0;
					if (tailPtr->next.compare_exchange_weak(next, (tagged_node_t)n)) {
						tail_.compare_exchange_strong(tail, (tagged_node_t)n);
						return true;
					}
				} else {
					tail_.compare_exchange_strong(tail, (tagged_node_t)tNext);
				}
			}	
		}
	}

  // pop without atomic operation
  bool UnsafePop(T &ret) {
		node *next = tag2nextNodePtr(head_);
		node *head = tag2nodePtr(head_);

		// check is the queue is empty
    if (!next || next == head) {
      return false;
    }

    ret = next->data;
		delete head;
    head_ = (tagged_node_t)next;
    return true;
  }

	bool Pop(T &ret) {
		for (;;) {
			tagged_node_t head = head_.load(memory_order_acquire);
			tagged_node_t tail = tail_.load(memory_order_acquire);

			node *next   = tag2nextNodePtr(head_);
			node *headPtr = tag2nodePtr(head_);

			tagged_node_t head2= head_.load(memory_order_acquire);
			if (likely(head == head2)) {
				if (head == tail) {
					if (next == 0) return false;
					tail_.compare_exchange_strong(tail, (tagged_node_t)next);
				} else {
					if (next == 0) continue;

					ret = next->data;
					if (head_.compare_exchange_weak(head, (tagged_node_t)next)) {
						delete headPtr;
						return true;
					}
				}
			}
		}
	}

private:
	node *tag2nodePtr(const atomic_ulong &a) {
		return reinterpret_cast<node *>(a.load(memory_order_acquire));
	}

	node *tag2nextNodePtr(const atomic_ulong &a) {
		node *n = tag2nodePtr(a);
		if (n) {
			return tag2nodePtr(n->next);
		}
		return 0;
	}

private:
	atomic_ulong head_;	
	char padding1[kPaddingSize];

	atomic_ulong tail_;
	char padding2[kPaddingSize];
};

};