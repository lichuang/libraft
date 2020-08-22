/*
 * Copyright (C) lichuang
 */

#include <mutex>
#include <condition_variable>

#pragma once

using namespace std;

// a simple WaitGroup class implement in C++ 11
namespace libraft {
class WaitGroup {
public:
  WaitGroup() : count_(0) {}
  ~WaitGroup() {}

  void Add(int delta) {
    unique_lock<mutex> lock(mutex_);
    
    count_ += delta;
  }

  void Done() {
    unique_lock<mutex> lock(mutex_);

    if (--count_ <= 0) {
      cv_.notify_all();
    }
  }

  void Wait() {
    unique_lock<mutex> lock(mutex_);

    while (count_ > 0) {
      cv_.wait(lock);
    }
  }

private:
  int count_;
  mutex mutex_;
  condition_variable cv_;
};
};