/*
 * Copyright (C) lichuang
 */

#include "base/buffer.h"
#include "base/errcode.h"
#include "base/object_pool.h"
#include "core/accept_message.h"
#include "core/epoll.h"
#include "core/application.h"
#include "core/session.h"
#include "core/socket.h"
#include "base/worker.h"

BEGIN_NAMESPACE

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

END_NAMESPACE