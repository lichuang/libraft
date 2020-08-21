/*
 * Copyright (C) lichuang
 */

#pragma once

#include <functional>
#include <map>

#include "base/define.h"
#include "base/typedef.h"

namespace libraft {

class IMessage;
class Worker;
class IEntity;

// entity reference composed by worker and entity id
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

  void Sendto(IEntity* dst, IMessage* msg);
  
  void Ask(const EntityRef& dstRef, IMessage* msg, MessageResponseFn fn);

  void HandleResponse(IMessage* msg);

  virtual void Handle(IMessage* msg) {}

protected:
  EntityRef ref_;
  typedef std::map<MessageId, MessageResponseFn> MessageResponseMap;
  MessageResponseMap resp_fn_map_;
};

// the global Sendto function, message src entity is the worker entity
extern void Sendto(IEntity* dst, IMessage* msg);
};