/*
 * Copyright (C) lichuang
 */

#include <map>
#include <sys/prctl.h>
#include "base/entity.h"
#include "base/event.h"
#include "base/event_loop.h"
#include "base/message.h"
#include "base/mailbox.h"
#include "base/typedef.h"
#include "base/wait.h"
#include "base/worker.h"

using namespace std;

namespace libraft {

class workerEntity : public IEntity {
public:
  workerEntity() {
  }

  virtual ~workerEntity() {
  }

  void Handle(IMessage* m) {
  }

};

// TLS worker pointer
thread_local static Worker* gWorker;

Worker::Worker(const string &name)
  : state_(kThreadNone),
    name_(name),
    thread_(nullptr),
    mailbox_(nullptr),
    ev_loop_(new EventLoop()),
    event_(nullptr),
    current_(0),
    worker_entity_(nullptr),
    current_msg_id_(0) {  
  mailbox_ = new Mailbox(this);
  // add mailbox signal fd into event loop
  fd_t fd = signaler_.Fd();

  event_ = new Event(ev_loop_, fd, this);
  event_->EnableRead();

  // worker entity is the id 1 entity in each worker
  worker_entity_ = new workerEntity();
  AddEntity(worker_entity_);

  // save TLS worker pointer
  gWorker = this;

  // start worker thread
  WaitGroup wg;
  wg.Add(1);
  thread_ = new std::thread(Worker::workerMain, this, &wg);
  wg.Wait();
}

Worker::~Worker() {
  delete mailbox_;
  delete ev_loop_;
  delete event_;
  delete worker_entity_;
  delete thread_;
}

void 
Worker::AddEntity(IEntity* entity) {
  std::lock_guard<std::mutex> lock(mutex_);

  // register entity in worker
  EntityId id = ++current_;
  while (entities_.find(id) != entities_.end()) {
    id++;
  }

  entity->ref_.id = id;
  entity->ref_.worker = this;
  entities_[id] = entity;
}

void
Worker::handleRead(Event*) {
  signaler_.Recv();
  mailbox_->Recv();

  if (!Running()) {
    ev_loop_->Stop();
  }
}

void
Worker::handleWrite(Event*) {
  // nothing to do
}

MessageId 
Worker::newMsgId() {
  return ++current_msg_id_;
}

void
Worker::processMsgInEntity(IMessage *msg) {
  const EntityRef& dstRef = msg->dstRef_;
  EntityId id = dstRef.id;

  EntityMap::iterator iter = entities_.find(id);
  if (iter == entities_.end()) {
    delete msg;
    return;
  }

  if (!msg->isResponse_) {
    iter->second->Handle(msg);
  } else {
    iter->second->HandleResponse(msg);
  }

  delete msg;
}

void
Worker::process(IMessage *msg) {
  processMsgInEntity(msg);
}

void 
Worker::notify() {
  signaler_.Send();
}

void
Worker::Send(IMessage *msg) {
  mailbox_->Send(msg);
}

void
Worker::init() {
  state_ = kThreadRunning;
  ::prctl(PR_SET_NAME, name_.c_str());
}

void
Worker::Run() {
  init();
  ev_loop_->Run();
}

void 
Worker::Stop() {
  state_ = kThreadStopping;
  signaler_.Send();
  thread_->join();
  mailbox_->Recv();
  state_ = kThreadStopped;
}

void 
Worker::workerMain(Worker* worker, WaitGroup* wg) {
  wg->Done();
  worker->Run();
}

void 
Sendto(const EntityRef& dstRef, IMessage* msg) {
  gWorker->worker_entity_->Sendto(dstRef, msg);
}

MessageId 
newMsgId() {
  return gWorker->newMsgId();
}

const string& 
CurrentThreadName() {
  return gWorker->String();
}

ThreadId CurrentThreadId() {
  return gWorker->Id();
}
};