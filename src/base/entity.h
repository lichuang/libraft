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

struct EntityRef {
  Worker *worker;
  EntityId id;

  EntityRef() : worker(nullptr), id(0) {

  }

  void Send(IMessage* msg);
  void Response(IMessage* msg, IMessage* srcMsg);

  EntityRef& operator =(const EntityRef& ref) {
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

  const EntityRef& Ref() const {
    return ref_;
  }

  void Send(IMessage* msg);
  
  void Ask(IMessage* msg, MessageResponseFn fn);

  virtual void Handle(IMessage* msg) = 0;
  void HandleResponse(IMessage* msg);

protected:
  EntityRef ref_;
  typedef std::map<int, MessageResponseFn> MessageResponseMap;
  MessageResponseMap resp_fn_map_;
};

};