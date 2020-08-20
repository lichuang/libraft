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

Worker::Worker(const string &name)
  : Thread(name),
    mailbox_(NULL),
    ev_loop_(new EventLoop()),
    event_(NULL),
    current_(0) {  
  mailbox_ = new Mailbox(this);
  // add mailbox signal fd into poller
  fd_t fd = signaler_.Fd();

  event_ = new Event(ev_loop_, fd, this);
  event_->EnableRead();
}

Worker::~Worker() {
  delete mailbox_;
  delete ev_loop_;
  delete event_;
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

};