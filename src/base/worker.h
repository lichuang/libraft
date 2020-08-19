/*
 * Copyright (C) lichuang
 */

#pragma once

#include <string>
#include "base/define.h"
#include "base/event.h"
#include "base/thread.h"

using namespace std;

namespace libraft {

class Event;
class EventLoop;

// worker thread
// inside the worker there is a mailbox,
// other threads can communicate to the thread using message though mailbox
class Worker : public Thread, public IEventHandler {
public:
  Worker(const string& name);
  virtual ~Worker();

  // process message handler
  virtual void Process(IMessage*);

  // send message to the worker
  void Send(IMessage *msg);
    
  virtual void handleRead(Event*);

  virtual void handleWrite(Event*);
    
protected:  
  virtual void Run();

protected:
  Mailbox *mailbox_;
  EventLoop *ev_loop_;
  Event* event_;
  DISALLOW_COPY_AND_ASSIGN(Worker);
};

};