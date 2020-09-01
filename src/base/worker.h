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
class Logger;
class Mailbox;
class IEntity;
class WaitGroup;
class workerEntity;
class WorkerPool;

typedef std::thread::id ThreadId;

extern void Sendto(const EntityRef& dstRef, IMessage* msg);
extern MessageId NewMsgId();
extern const string& CurrentThreadName();
extern ThreadId CurrentThreadId();

enum threadType {
  kMainThread = 1,
  kWorkThread = 2,
  kLogThread  = 3,
};

// worker thread
// inside each worker there is a mailbox,
// other threads can communicate to the thread using message though mailbox
class Worker : public IIOHandler {
  friend class Mailbox;
  friend class Logger;
  friend class workerEntity;
  friend class WorkerPool;

  friend void Sendto(const EntityRef& dstRef, IMessage* msg);
  friend MessageId NewMsgId();
  friend void initMainWorker();

public:
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

  virtual void onRead(IOEvent*);

  virtual void onWrite(IOEvent*);

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

  threadType Type() const {
    return type_;
  }

private:
  // worker can only be created in worker poll and logger
  Worker(const string& name, threadType, bool isMain = false);
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
  void addTimer(ITimerEvent* event);
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

  EntityId current_;

  typedef map<EntityId, IEntity*> EntityMap;
  EntityMap entities_;

  // default worker entity
  IEntity *worker_entity_;

  // currrent message id
  MessageId current_msg_id_;

  // timer event
  TimerEventId current_timer_id_;
  typedef map<TimerEventId, ITimerEvent*> TimerEventMap;
  TimerEventMap timer_event_map_;

  // thread type
  threadType type_;
  DISALLOW_COPY_AND_ASSIGN(Worker);
};

extern void initMainWorker();

extern bool InMainThread();
extern Worker* CurrentThread();

};