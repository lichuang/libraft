/*
 * Copyright (C) lichuang
 */

#pragma once

#include "base/define.h"
#include "base/typedef.h"

namespace libraft {

class Message;
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

  void Send(Message* msg) {

  }

  void Ask(Message* msg, int timeout, Message* response) { 

  }

  virtual void Handle(Message* msg) = 0;

protected:
  EntityRef ref_;
};

};