/*
 * Copyright (C) codedump
 */

#ifndef __LIBRAFT_BASE_ENTITY_H__
#define __LIBRAFT_BASE_ENTITY_H__

#include "base/define.h"
#include "base/typedef.h"

BEGIN_NAMESPACE

class Message;

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
  EntityId id_;
};

END_NAMESPACE

#endif  // __LIBRAFT_BASE_ENTITY_H__