/*
 * Copyright (C) lichuang
 */

#pragma once

#include <string>
#include "base/define.h"

using namespace std;

BEGIN_NAMESPACE

// worker thread
// inside the worker there is a mailbox,
// other threads can communicate to the thread using message though mailbox
class Worker 
  : public Thread {
public:
  Worker(const string& name);
  virtual ~Worker();

  // process message handler
  virtual void Process(Message*);

  Poller* GetPoller() {
    return poller_;
  }

  // send message to the worker
  void Send(Message *msg);
  
protected:  
  virtual void Run();

protected:
  Mailbox mailbox_;

  DISALLOW_COPY_AND_ASSIGN(Worker);
};

END_NAMESPACE