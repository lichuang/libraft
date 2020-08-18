/*
 * Copyright (C) lichuang
 */

#pragma once

#include <functional>
#include <string>
#include <pthread.h>

#include "base/define.h"

using namespace std;

namespace libraft {

typedef pthread_t tid_t;

enum ThreadState {
  kThreadNone,
  kThreadRunning,
  kThreadStopped,
};

typedef std::function<void ()> ThreadCallback;

class Thread {
public:
  Thread(const string& name);
  Thread(const string& name, ThreadCallback);

  virtual ~Thread();

  void Join();

  tid_t GetTid() const {
    return tid_;
  }

  const string& Name() const {
    return name_;
  }

  ThreadState State() const { 
    return state_; 
  }

  bool Running() const { 
    return state_ == kThreadRunning;
  }

  int Start();

private:
  static void* main(void *arg);

protected:
  virtual void Run() {
    
  }

private:
  tid_t tid_;
  string name_;
  ThreadState state_;
  ThreadCallback callback_;
};

extern const string& CurrentThreadName();

};