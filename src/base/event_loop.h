/*
 * Copyright (C) lichuang
 */

#pragma once

#include "base/define.h"
#include "base/libevent.h"

namespace libraft {

class IOEvent;

class EventLoop {
public:
  EventLoop();

  ~EventLoop();

  void Run();

  // Stop MUST be called in event loop
  void Stop();
  
  void* EventBase() {
    return ev_base_;
  }

  void Add(IOEvent*, int flags);  

private:
  struct event_base *ev_base_;

  DISALLOW_COPY_AND_ASSIGN(EventLoop);
};

};