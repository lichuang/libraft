/*
 * Copyright (C) lichuang
 */

#include <string.h>
#include "base/event_loop.h"
#include "base/event.h"
#include "base/libevent.h"

namespace libraft {

IEvent::IEvent(EventLoop* loop, fd_t fd, IEventHandler *handler)
  : loop_(loop),
    fd_(fd),
    handler_(handler),
    flags_(kNone),
    attached_(false) {
  event_ = new event;
  memset(event_, 0, sizeof(struct event));  
}

IEvent::~IEvent() {
  Close();
}

void
IEvent::Close() {
  if (event_) {
    event_del(event_);
    delete event_;
    event_ = NULL;    
  }
}

void 
IEvent::DetachFromLoop() {
  if (event_del(event_) == 0) {
    attached_ = false;
  }
}

void 
IEvent::AttachToLoop() {
    if (attached_) {
      DetachFromLoop();
    }

    ::event_assign(event_, (struct event_base *)loop_->EventBase(), fd_, 
                flags_ | EV_PERSIST, &IEvent::Handle, this);
    if (::event_add(event_, NULL) == 0) {
        attached_ = true;
    }
}

void 
IEvent::updateEventLoop() {
  if (IsNoneEvent()) {
    DetachFromLoop();
  } else {
    AttachToLoop();
  }
}

void 
IEvent::EnableRead() {
  int flags = flags_;
  flags_ |= kReadable;

  if (flags_ != flags) {
    updateEventLoop();
  }
}

void 
IEvent::EnableWrite() {
  int flags = flags_;
  flags_ |= kWritable;

  if (flags_ != flags) {
    updateEventLoop();
  }
}

void 
IEvent::DisableRead() {
  int flags = flags_;
  flags_ &= (~kReadable);

  if (flags_ != flags) {
    updateEventLoop();
  }
}

void 
IEvent::DisableWrite() {
  int flags = flags_;
  flags_ &= (~kWritable);

  if (flags_ != flags) {
    updateEventLoop();
  }
}

void 
IEvent::DisableAllEvent() {
  if (flags_ == kNone) {
    return;
  }

  flags_ = kNone;
  updateEventLoop();
}

void 
IEvent::Handle(fd_t fd, short which) {
  if ((which & kReadable) && event_) {
    handler_->handleRead(this);
  }

  if ((which & kWritable) && event_) {
    handler_->handleWrite(this);
  }
}

void 
IEvent::Handle(fd_t fd, short which, void* v) {
  IEvent* ev = (IEvent*)v;
  ev->Handle(fd, which);
}

};
