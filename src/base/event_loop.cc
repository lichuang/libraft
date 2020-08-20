/*
 * Copyright (C) lichuang
 */

#include "base/event_loop.h"

namespace libraft {

EventLoop::EventLoop()
  : ev_base_(NULL) {
  ev_base_ = event_base_new();
}

EventLoop::~EventLoop() {
  if (ev_base_) {
    event_base_free(ev_base_);
  }
}

void
EventLoop::Run() {
  event_base_dispatch(ev_base_);
}

void 
EventLoop::Stop() {
  event_base_loopexit(ev_base_, NULL);
}

};