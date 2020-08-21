/*
 * Copyright (C) lichuang
 */

#pragma once

#include <map>
#include <mutex>
#include <string>
#include <thread>
#include "base/define.h"
#include "base/event.h"
#include "base/signaler.h"

using namespace std;

namespace libraft {

class Event;
class EventLoop;
class Mailbox;
class IEntity;
class WaitGroup;

typedef std::thread::id ThreadId;

extern void Sendto(const EntityRef& dstRef, IMessage* msg);
extern MessageId newMsgId();
extern const string& CurrentThreadName();
extern ThreadId CurrentThreadId();

// worker thread
// inside each worker there is a mailbox,
// other threads can communicate to the thread using message though mailbox
class Worker : public IEventHandler {
  friend class Mailbox;

  friend void Sendto(const EntityRef& dstRef, IMessage* msg);
  friend MessageId newMsgId();

public:
  Worker(const string& name);
  virtual ~Worker();

  void AddEntity(IEntity*);

  // send message to the worker
  void Send(IMessage *msg);    

  virtual void handleRead(Event*);

  virtual void handleWrite(Event*);

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
  void process(IMessage*);
  void processMsgInEntity(IMessage*);
  void notify();

  MessageId newMsgId();

  // after thread created, init thread info
  void init();

protected:  
  virtual void Run();
  static void workerMain(Worker*, WaitGroup*);

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
  Event* event_;
  
  // Signaler to pass signals from writer thread to reader thread.
  Signaler signaler_;

  // protect entity register operation
  std::mutex mutex_;
  EntityId current_;

  typedef map<EntityId, IEntity*> EntityMap;
  EntityMap entities_;

  // default worker entity
  IEntity *worker_entity_;

  // message id
  MessageId current_msg_id_;
  DISALLOW_COPY_AND_ASSIGN(Worker);
};

};