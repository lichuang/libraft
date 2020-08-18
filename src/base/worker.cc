/*
 * Copyright (C) lichuang
 */

#include "base/worker.h"

namespace libraft {

Worker::Worker(const string &name)
  : Thread(name),
    poller_(new Epoll()) {
  int rc = poller_->Init(1024);
  if (rc != kOK) {
    return;
  }

  // add mailbox signal fd into poller
  fd_t fd = mailbox_.Fd();
  handle_ = poller_->Add(fd, this);
}

Worker::~Worker() {
  delete poller_;
}

void
Worker::In() {
  Message* msg;
  int rc = mailbox_.Recv(&msg, 0);

  while (rc == 0 || errno == EINTR) {
    if (rc == 0)  {
      msg->Process();
      delete msg;
    }
    rc = mailbox_.Recv(&msg, 0);
  }
}

void
Worker::Out() {
  // nothing to do
}

void
Worker::Timeout() {
  // nothing to do
}

void
Worker::Process(Message *msg) {
}

void
Worker::Send(Message *msg) {
  mailbox_.Send(msg);
}

void
Worker::Run() {
  while (Running()) {
    poller_->Dispatch();
  }
}

};