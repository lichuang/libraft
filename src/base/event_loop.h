/*
 * Copyright (C) lichuang
 */

#pragma once

#include "base/define.h"
#include "base/libevent.h"

namespace libraft {

class Event;

class EventLoop {
public:
  EventLoop();

  ~EventLoop();

  void Run();

  void* EventBase() {
    return ev_base_;
  }

  void Add(Event*, int flags);
  
private:
  struct event_base *ev_base_;

  DISALLOW_COPY_AND_ASSIGN(EventLoop);
};

};