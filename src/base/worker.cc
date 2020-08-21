/*
 * Copyright (C) lichuang
 */

#include "base/entity.h"
#include "base/event.h"
#include "base/event_loop.h"
#include "base/message.h"
#include "base/mailbox.h"
#include "base/typedef.h"
#include "base/worker.h"
#include <map>

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

thread_local static Worker* gWorker;

Worker::Worker(const string &name)
  : Thread(name),
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
}

Worker::~Worker() {
  delete mailbox_;
  delete ev_loop_;
  delete event_;
  delete worker_entity_;
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
Worker::Run() {
  ev_loop_->Run();
}

void 
Worker::Stop() {
  state_ = kThreadStopping;
  signaler_.Send();
  Join();
  mailbox_->Recv();
}

void 
Sendto(IEntity* dst, IMessage* msg) {
  gWorker->worker_entity_->Sendto(dst, msg);
}

MessageId 
newMsgId() {
  return gWorker->newMsgId();
}

};