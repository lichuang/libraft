/*
 * Copyright (C) lichuang
 */

#include "base/event.h"
#include "base/event_loop.h"
#include "base/message.h"
#include "base/mailbox.h"
#include "base/worker.h"

namespace libraft {

Worker::Worker(const string &name)
  : Thread(name),
    mailbox_(NULL),
    ev_loop_(new EventLoop()),
    event_(NULL) {  
  mailbox_ = new Mailbox(this);
  // add mailbox signal fd into poller
  fd_t fd = mailbox_->Fd();

  event_ = new Event(ev_loop_, fd, this);
  event_->EnableRead();
}

Worker::~Worker() {
  delete mailbox_;
  delete ev_loop_;
  delete event_;
}

void
Worker::handleRead(Event*) {
  mailbox_->Recv();
}

void
Worker::handleWrite(Event*) {
  // nothing to do
}

void
Worker::process(IMessage *msg) {
}

void
Worker::Send(IMessage *msg) {
  mailbox_->Send(msg);
}

void
Worker::Run() {
  ev_loop_->Run();
}

};