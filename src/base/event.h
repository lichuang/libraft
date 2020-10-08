/*
 * Copyright (C) lichuang
 */

#pragma once

#include <map>
#include "base/duration.h"
#include "base/typedef.h"

using namespace std;
struct event;

namespace libraft {

class IOEvent;
class ITimerEvent;
class EventLoop;
class ITimerHandler;

enum EventFlag {
  kNone     = 0x00,
  kReadable = 0x02,
  kWritable = 0x04,
};

// virtual class for event
class IEvent {
public:
  IEvent(EventLoop *evloop);
  virtual ~IEvent();

  void Close();

protected:
  struct event *event_;
  EventLoop* ev_loop_;

  // if or not attached to a event loop
  bool attached_;
};

// virtual class for io event handler
class IIOHandler {
public:
  virtual ~IIOHandler() {}

  virtual void onRead(IOEvent*) = 0;

  virtual void onWrite(IOEvent*) = 0;
};

// class for IO events
class IOEvent : public IEvent {
public:
  IOEvent(EventLoop*, fd_t, IIOHandler*);
  virtual ~IOEvent();  

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
  
private:
  void Handle(fd_t fd, short which);
  static void Handle(fd_t fd, short which, void* v);

  void AttachToLoop();
  void DetachFromLoop();

  void updateEventLoop();

protected:
  fd_t fd_;

  IIOHandler* handler_;
  // event flags
  int flags_;
};

// class for timer events
class ITimerEvent : public IEvent {
public:
  ITimerEvent(EventLoop *, ITimerHandler *, const Duration& timeout, bool, TimerEventId);

  void Start();

  TimerEventId Id() const {
    return id_;
  }

private:
  static void Handle(fd_t fd, short which, void* v);

protected:
  // if this timer is called periodic or once?
  bool once_;

  ITimerHandler* handler_;
  Duration timeout_;

  TimerEventId id_;
};

class ITimerHandler {
public:
  virtual ~ITimerHandler() {}

  virtual void onTimeout(ITimerEvent*) {}

  void AddTimer(TimerEventId id, ITimerEvent* event) {
    event_map_[id] = event;
  }

  void DelTimer(TimerEventId id) {
    delete event_map_[id];
    event_map_.erase(id);
  }

protected:
  typedef map<TimerEventId, ITimerEvent*> TimerMap;
  TimerMap event_map_;
};

};  // namespace libraft
