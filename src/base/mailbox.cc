/*
 * Copyright (C) lichuang
 */

#include "base/error.h"
#include "base/mailbox.h"
#include "base/message.h"

namespace libraft {

Mailbox::Mailbox()
  : active_(false) {
  pipe_.CheckRead();
}

Mailbox::~Mailbox() {
}

void Mailbox::Send(IMessage *msg) {
  // multi write single read mode
  sync_.Lock();
  pipe_.Write(msg, false);
  bool ok = pipe_.Flush();
  sync_.UnLock();
  if (!ok) {
    // Flush return false means the reader thread is sleeping,
    // so send a signal to wake up the reader
    signaler_.Send();
  }
}

int
Mailbox::Recv(IMessage** msg, int timeout) {
  // Try to get the command straight away.
  if (active_) {
    if (pipe_.Read(msg)) {
      // if read success, return
      return kOK;
    }
    // If there are no more commands available, switch into passive state.
    active_ = false;
  }
  //  Wait for signal from the command sender.
  int rc = signaler_.Wait(timeout);
  if (rc == -1) {
    return kError;
  }

  //  Receive the signal.
  rc = static_cast<int>(signaler_.RecvFailable());
  if (rc == -1) {
    return kError;
  }

  //  Switch into active state.
  active_ = true;
  pipe_.Read(msg);
  return kOK;
}

};