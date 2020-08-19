/*
 * Copyright (C) lichuang
 */

#include <string.h>
#include "base/event_loop.h"
#include "base/event.h"
#include "base/libevent.h"

namespace libraft {

Event::Event(EventLoop* loop, fd_t fd, IEventHandler *handler)
  : loop_(loop),
    fd_(fd),
    handler_(handler),
    flags_(kNone),
    attached_(false) {
  event_ = new event;
  memset(event_, 0, sizeof(struct event));  
}

Event::~Event() {
  Close();
}

void
Event::Close() {
  if (event_) {
    event_del(event_);
    delete event_;
    event_ = NULL;    
  }
}

void 
Event::DetachFromLoop() {
  if (event_del(event_) == 0) {
    attached_ = false;
  }
}

void 
Event::AttachToLoop() {
  if (attached_) {
    DetachFromLoop();
  }

  ::event_assign(event_, (struct event_base *)loop_->EventBase(), fd_, 
                flags_ | EV_PERSIST, &Event::Handle, this);
  if (::event_add(event_, NULL) == 0) {
    attached_ = true;
  }
}

void 
Event::updateEventLoop() {
  if (IsNoneEvent()) {
    DetachFromLoop();
  } else {
    AttachToLoop();
  }
}

void 
Event::EnableRead() {
  int flags = flags_;
  flags_ |= kReadable;

  if (flags_ != flags) {
    updateEventLoop();
  }
}

void 
Event::EnableWrite() {
  int flags = flags_;
  flags_ |= kWritable;

  if (flags_ != flags) {
    updateEventLoop();
  }
}

void 
Event::DisableRead() {
  int flags = flags_;
  flags_ &= (~kReadable);

  if (flags_ != flags) {
    updateEventLoop();
  }
}

void 
Event::DisableWrite() {
  int flags = flags_;
  flags_ &= (~kWritable);

  if (flags_ != flags) {
    updateEventLoop();
  }
}

void 
Event::DisableAllEvent() {
  if (flags_ == kNone) {
    return;
  }

  flags_ = kNone;
  updateEventLoop();
}

void 
Event::Handle(fd_t fd, short which) {
  if ((which & kReadable) && event_) {
    handler_->handleRead(this);
  }

  if ((which & kWritable) && event_) {
    handler_->handleWrite(this);
  }
}

void 
Event::Handle(fd_t fd, short which, void* v) {
  Event* ev = (Event*)v;
  ev->Handle(fd, which);
}

};
