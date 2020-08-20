/*
 * Copyright (C) lichuang
 */

#pragma once

#include <map>
#include <mutex>
#include <string>
#include "base/define.h"
#include "base/event.h"
#include "base/signaler.h"
#include "base/thread.h"

using namespace std;

namespace libraft {

class Event;
class EventLoop;
class Mailbox;
class IEntity;

// worker thread
// inside the worker there is a mailbox,
// other threads can communicate to the thread using message though mailbox
class Worker : public Thread, public IEventHandler {
  friend class Mailbox;
public:
  Worker(const string& name);
  virtual ~Worker();

  void AddEntity(IEntity*);

  // send message to the worker
  void Send(IMessage *msg);
    
  virtual void handleRead(Event*);

  virtual void handleWrite(Event*);

  void Stop();

private:
  void process(IMessage*);
  void processMsgInEntity(IMessage*);
  void notify();
  
protected:  
  virtual void Run();

protected:
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

  DISALLOW_COPY_AND_ASSIGN(Worker);
};

extern void     registerEntiry(EntityId id, IEntity*);
};