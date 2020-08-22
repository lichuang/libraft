/*
 * Copyright (C) lichuang
 */

#pragma once

#include <map>
#include <mutex>
#include <string>
#include <thread>
#include "base/define.h"
#include "base/entity.h"
#include "base/event.h"
#include "base/duration.h"
#include "base/signaler.h"

using namespace std;

namespace libraft {

class IOEvent;
class ITimerHandler;
class EventLoop;
class Mailbox;
class IEntity;
class WaitGroup;
class workerEntity;

typedef std::thread::id ThreadId;

extern void Sendto(const EntityRef& dstRef, IMessage* msg);
extern MessageId NewMsgId();
extern const string& CurrentThreadName();
extern ThreadId CurrentThreadId();

// worker thread
// inside each worker there is a mailbox,
// other threads can communicate to the thread using message though mailbox
class Worker : public IIOEventHandler {
  friend class Mailbox;
  friend class workerEntity;

  friend void Sendto(const EntityRef& dstRef, IMessage* msg);
  friend MessageId NewMsgId();

public:
  Worker(const string& name);
  virtual ~Worker();

  void AddEntity(IEntity*);

  // send message to the entity bound in this worker
  void Send(IMessage *msg);    
  
  // send message to the worker,it will be handled in default workerEntity
  void SendtoWorker(IMessage *msg);

  // create and return a periodic timer id
  TimerEventId RunEvery(ITimerHandler*, const Duration& internal);

  // create and return a fire-once timer id
  TimerEventId RunOnce(ITimerHandler*, const Duration& delay);

  virtual void handleRead(IOEvent*);

  virtual void handleWrite(IOEvent*);

  void Stop();

  ThreadId Id() {
    return thread_->get_id();
  }

  const string& String() {
    return name_;
  }

  bool Running() const { 
    return state_ == kThreadRunning;
  }

private:
  bool runningInWorker();

  void process(IMessage*);

  // nofity the worker there is new msg
  void notify();

  MessageId NewMsgId();

  // after thread created, init thread info
  void init();

  TimerEventId newTimer(ITimerHandler*, const Duration& delay, bool);

protected:  
  virtual void Run();
  static void main(Worker*, WaitGroup*);
  void addTimer(TimerEvent* event);
  TimerEventId newTimerEventId();

private:
  enum ThreadState {
    kThreadNone,
    kThreadRunning,
    kThreadStopping,
    kThreadStopped,
  };

protected:
  ThreadState state_;
  std::string name_;
  std::thread *thread_;
  Mailbox *mailbox_;
  EventLoop *ev_loop_;

  // signal event 
  IOEvent* event_;
  
  // Signaler to pass signals from writer thread to reader thread.
  Signaler signaler_;

  // protect entity register operation
  std::mutex mutex_;
  EntityId current_;

  typedef map<EntityId, IEntity*> EntityMap;
  EntityMap entities_;

  // default worker entity
  IEntity *worker_entity_;

  // currrent message id
  MessageId current_msg_id_;

  // timer event
  TimerEventId current_timer_id_;
  typedef map<TimerEventId, TimerEvent*> TimerEventMap;
  TimerEventMap timer_event_map_;

  DISALLOW_COPY_AND_ASSIGN(Worker);
};

};