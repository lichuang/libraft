/*
 * Copyright (C) codedump
 */
#ifndef __LIBRAFT_BASE_WORKER_H__
#define __LIBRAFT_BASE_WORKER_H__

#include <string>
#include "base/define.h"

using namespace std;

BEGIN_NAMESPACE

// worker thread
// inside the worker there is a mailbox,
// other threads can communicate to the thread using message though mailbox
class Worker 
  : public Thread,
    public Event,
    public MessageHandler {
public:
  Worker(const string& name);
  virtual ~Worker();

  // Event vitual methods
  virtual void In();

  virtual void Out();

  virtual void Timeout();

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
  DISALLOW_COPY_AND_ASSIGN(Worker);
};

END_NAMESPACE

#endif  // __LIBRAFT_BASE_WORKER_H__
