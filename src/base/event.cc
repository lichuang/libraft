/*
 * Copyright (C) lichuang
 */

#include <string.h>
#include "base/event_loop.h"
#include "base/event.h"
#include "base/libevent.h"

namespace libraft {

IEvent::IEvent(EventLoop *evloop)
  : ev_loop_(evloop),
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
    event_ = nullptr;    
  }
}

IOEvent::IOEvent(EventLoop* loop, fd_t fd, IIOHandler *handler)
  : IEvent(loop),
    fd_(fd),
    handler_(handler),
    flags_(kNone) {
}

IOEvent::~IOEvent() {
  Close();
}

void 
IOEvent::AttachToLoop() {
  if (attached_) {
    DetachFromLoop();
  }

  ::event_assign(event_, (struct event_base *)ev_loop_->EventBase(), fd_, 
                flags_ | EV_PERSIST, &IOEvent::Handle, this);
                
  if (::event_add(event_, NULL) == 0) {
    attached_ = true;
  }
}

void 
IOEvent::DetachFromLoop() {
  if (event_del(event_) == 0) {
    attached_ = false;
  }
}

void 
IOEvent::updateEventLoop() {
  if (IsNoneEvent()) {
    DetachFromLoop();
  } else {
    AttachToLoop();
  }
}

void 
IOEvent::EnableRead() {
  int flags = flags_;
  flags_ |= kReadable;

  if (flags_ != flags) {
    updateEventLoop();
  }
}

void 
IOEvent::EnableWrite() {
  int flags = flags_;
  flags_ |= kWritable;

  if (flags_ != flags) {
    updateEventLoop();
  }
}

void 
IOEvent::DisableRead() {
  int flags = flags_;
  flags_ &= (~kReadable);

  if (flags_ != flags) {
    updateEventLoop();
  }
}

void 
IOEvent::DisableWrite() {
  int flags = flags_;
  flags_ &= (~kWritable);

  if (flags_ != flags) {
    updateEventLoop();
  }
}

void 
IOEvent::DisableAllEvent() {
  if (flags_ == kNone) {
    return;
  }

  flags_ = kNone;
  updateEventLoop();
}

void 
IOEvent::Handle(fd_t fd, short which) {
  if ((which & kReadable) && event_) {
    handler_->onRead(this);
  }

  if ((which & kWritable) && event_) {
    handler_->onWrite(this);
  }
}

void 
IOEvent::Handle(fd_t fd, short which, void* v) {
  IOEvent* ev = (IOEvent*)v;
  ev->Handle(fd, which);
}

ITimerEvent::ITimerEvent(EventLoop *loop, ITimerHandler *handler, const Duration& timeout, bool once, TimerEventId id)
  : IEvent(loop),
    once_(once),
    handler_(handler),
    timeout_(timeout),
    id_(id) {
  handler->AddTimer(id, this);
}

void
ITimerEvent::Start() {
  struct timeval tv;
  timeout_.To(&tv);

  ::event_assign(event_, (struct event_base *)ev_loop_->EventBase(), -1, 
                 once_ ? 0 : EV_PERSIST, &ITimerEvent::Handle, this);  
  ::event_add(event_, &tv);
}

void
ITimerEvent::Handle(fd_t, short ,void *v) {
  ITimerEvent* ev = (ITimerEvent*)v;
  ev->handler_->onTimeout(ev);
  if (ev->once_) {
    ev->handler_->DelTimer(ev->id_);
  }
}
};
