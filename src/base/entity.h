/*
 * Copyright (C) lichuang
 */

#pragma once

#include <functional>
#include <map>

#include "base/define.h"
#include "base/typedef.h"

// class Entity is the core element in this server framework
// each Entity is bind in a worker thread, it can send message through worker mailbox to other Entity

namespace libraft {

class IMessage;
class Worker;
class IEntity;

// entity reference is composed by worker and entity id
struct EntityRef {
  Worker *worker;
  EntityId id;

  EntityRef() : worker(nullptr), id(0) {
  }

  void Send(IMessage* msg);
  void Response(IMessage* msg, IMessage* srcMsg);

  EntityRef& operator= (const EntityRef& ref) {
    worker = ref.worker;
    id = ref.id;
    return *this;
  }
};

class IEntity {
  friend class Worker;
  typedef std::function<void(const IMessage*)> MessageResponseFn;

public:
  IEntity() { 
  }

  virtual ~IEntity() {
  }

  const EntityRef& Ref() const {
    return ref_;
  }

  // send a message to dst entity, unlike Ask, Sendto has no response
  void Sendto(const EntityRef& dstRef, IMessage* msg);
  
  // ask dst entity something, and the callback fn will be called when receive response
  void Ask(const EntityRef& dstRef, IMessage* msg, MessageResponseFn fn);

  // src entity handle response from dst
  void HandleResponse(IMessage* msg);

  virtual void Handle(IMessage* msg) {}

protected:
  EntityRef ref_;

  typedef std::map<MessageId, MessageResponseFn> MessageResponseMap;
  // when receive a response, entity get callback fn in resp_fn_map_ by message id
  MessageResponseMap resp_fn_map_;
};

// if use this global Sendto function, message src entity is the worker entity
extern void Sendto(const EntityRef& dstRef, IMessage* msg);
};