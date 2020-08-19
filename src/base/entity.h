/*
 * Copyright (C) lichuang
 */

#pragma once

#include "base/define.h"
#include "base/typedef.h"

namespace libraft {

class IMessage;
class Mailbox;

struct EntityRef {
  class Mailbox;
  EntityId id_;
};

class Entity {
public:
  Entity() { 
    // register in worker
  }

  void Send(IMessage* msg) {

  }

  void Ask(IMessage* msg, int timeout, IMessage* response) { 

  }

  virtual void Handle(IMessage* msg) = 0;

protected:
  EntityRef ref_;
};

};