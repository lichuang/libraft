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

class EventLoop;
class IMessage;
class Worker;
class IEntity;

// entity reference is composed by worker and entity id
struct EntityRef {
  EntityRef() : worker_(nullptr), id_(0) {
  }

  EntityId Id() const {
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
  EntityType type_;  
};

// the message handler function type
typedef std::function<void(IMessage*)> MessageFunc;

// the message response handler function type
typedef std::function<void(const IMessage*)> MessageResponseFn;

class IEntity : public ITimerHandler {
  friend class Worker;
  
public:
  IEntity(EntityType typ);

  // cannot delete directly
  virtual ~IEntity();
  
  // do init in binding worker, can be re-implemented by subclass
  virtual void initAfterBind() {}

  // can be re-implemented by subclass
  virtual int64_t Hash() const {
    return ref_.id_;
  }

  const EntityRef& Ref() const {
    return ref_;
  }

  EntityType Type() const {
    return ref_.type_;
  }
  
  // return true is current thread is the entity binding thread
  bool InSameWorker();

  void Bind(Worker *w, EntityId id);

  // send a message to dst entity, unlike Ask, Sendto has no response
  void Sendto(const EntityRef& dstRef, IMessage* msg);
  
  // send a message to this entity, unlike Ask, Sendto has no response
  void Send(IMessage* msg);

  // ask dst entity something, and the callback fn will be called when receive response
  void Ask(const EntityRef& dstRef, IMessage* msg, MessageResponseFn fn);

  // src entity handle response from dst
  void HandleResponse(IMessage* msg);

  void AddTimer();

  void RegisterMessageHandler(MessageType, MessageFunc fn);

  virtual void Handle(IMessage* msg);

  virtual void onTimeout(ITimerEvent*) {}

protected:
  EntityRef ref_;

  typedef std::map<MessageId, MessageResponseFn> MessageRespFuncMap;
  // when receive a response, entity get callback fn in resp_func_map_ by message id
  MessageRespFuncMap resp_func_map_;

  typedef std::map<MessageType, MessageFunc> MessageFuncMap;
  MessageFuncMap msg_func_map_;
};

// if use this global Sendto function, message src entity is the worker entity
extern void Sendto(const EntityRef& dstRef, IMessage* msg);
};