/*
 * Copyright (C) lichuang
 */

#pragma once

#include "base/typedef.h"

class EventLoop;
struct event;

namespace libraft {

class Event;

class EventHandler {
public:
  virtual ~EventHandler() {}

  virtual void handleRead(Event*) = 0;

  virtual void handleWrite(Event*) = 0;
};

// virtual class for notify events
class Event {
public:
  enum EventFlag {
    kNone     = 0x00,
    kReadable = 0x02,
    kWritable = 0x04,
  };

  Event(EventLoop*, fd_t, EventHandler*);
  virtual ~Event();

  void* EventData() { return event_; }
  
  void Close();

  void EnableRead();
  void EnableWrite();
  void DisableRead();
  void DisableWrite();
  void DisableAllEvent();

  bool IsReadable() const {
    return (flags_ & kReadable) != 0;
  }
    
  bool IsWritable() const {
    return (flags_ & kWritable) != 0;
  }

  bool IsNoneEvent() const {
    return flags_ == kNone;
  }
  
  void AttachToLoop();

private:
  void Handle(fd_t fd, short which);
  static void Handle(fd_t fd, short which, void* v);

  void updateEventLoop();
  void DetachFromLoop();

protected:
  struct event *event_;
  EventLoop* loop_;
  fd_t fd_;

  EventHandler* handler_;
  // event flags
  int flags_;

  // if or not attached to a event loop
  bool attached_;
};

};  // namespace serverkit
