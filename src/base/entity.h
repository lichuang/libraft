/*
 * Copyright (C) lichuang
 */

#pragma once

#include <functional>
#include <map>

#include "base/event.h"
#include "base/define.h"
#include "base/typedef.h"

// class Entity is the core element in this server framework
// each Entity is binded in a worker thread, it can send message through worker mailbox to other Entity

namespace libraft {

class IMessage;
class Worker;
class IEntity;

// entity reference is composed by worker and entity id
struct EntityRef {
  EntityRef() : worker_(nullptr), id_(0) {
  }

  EntityId id() const {
    return id_;
  }

  void Send(IMessage* msg);
  void Response(IMessage* msg, IMessage* srcMsg);

  EntityRef& operator= (const EntityRef& ref) {
    worker_ = ref.worker_;
    id_ = ref.id_;
    return *this;
  }

  Worker *worker_;
  EntityId id_;  
};

class IEntity : public ITimerHandler {
  friend class Worker;
  typedef std::function<void(const IMessage*)> MessageResponseFn;

public:
  IEntity(Worker*);

  virtual ~IEntity() {
  }

  const EntityRef& Ref() const {
    return ref_;
  }

  void Bind(Worker *w, EntityId id) {
    ref_.worker_ = w;
    ref_.id_ = id;
  }

  // send a message to dst entity, unlike Ask, Sendto has no response
  void Sendto(const EntityRef& dstRef, IMessage* msg);
  
  // ask dst entity something, and the callback fn will be called when receive response
  void Ask(const EntityRef& dstRef, IMessage* msg, MessageResponseFn fn);

  // src entity handle response from dst
  void HandleResponse(IMessage* msg);

  void AddTimer();

  virtual void Handle(IMessage* msg) {}

  virtual void onTimeout(TimerEvent*) {}

protected:
  EntityRef ref_;

  typedef std::map<MessageId, MessageResponseFn> MessageResponseMap;
  // when receive a response, entity get callback fn in resp_fn_map_ by message id
  MessageResponseMap resp_fn_map_;
};

// if use this global Sendto function, message src entity is the worker entity
extern void Sendto(const EntityRef& dstRef, IMessage* msg);
};