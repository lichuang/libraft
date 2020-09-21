/*
 * Copyright (C) lichuang
 */

#include "base/entity.h"
#include "base/log.h"
#include "base/message.h"
#include "base/worker.h"

namespace libraft {

void 
EntityRef::Send(IMessage* msg) {
  worker_->Send(msg);
}

void 
EntityRef::Response(IMessage* msg, IMessage* srcMsg) {
  msg->responseFor(srcMsg);
  worker_->Send(msg);
}

IEntity::IEntity(EntityType typ) {
  ref_.type_ = typ;
}

IEntity::~IEntity() {
  //ASSERT(CurrentThread() == ref_.worker_) << "entity MUST be deleted in the bound worker";
}

bool 
IEntity::InSameWorker() {
  return CurrentThread() == ref_.worker_;
}

void 
IEntity::Bind(Worker *w, EntityId id) {
  ref_.worker_ = w;
  ref_.id_ = id;
}

void 
IEntity::Ask(const EntityRef& dstRef, IMessage* msg, MessageResponseFn fn) {
  resp_func_map_[msg->id_] = fn;
  msg->setDstEntiity(dstRef);
  msg->setSrcEntiity(ref_);
  dstRef.worker_->Send(msg);  
}

void 
IEntity::RegisterMessageHandler(MessageType typ, MessageFunc fn) {
  msg_func_map_[typ] = fn;
}

void 
IEntity::Handle(IMessage* msg) {
  MessageType typ = msg->Type();
  MessageFuncMap::iterator iter = msg_func_map_.find(typ);
  if (iter == msg_func_map_.end()) {
    return;
  }

  iter->second(msg);
}

void 
IEntity::HandleResponse(IMessage* msg) {
  MessageRespFuncMap::iterator iter = resp_func_map_.find(msg->id_);
  if (iter == resp_func_map_.end()) {
    return;
  }
  iter->second(msg);
  resp_func_map_.erase(msg->id_);
}

void 
IEntity::Send(IMessage* msg) {
  msg->setDstEntiity(ref_);
  ref_.worker_->Send(msg);
}

void 
IEntity::Sendto(const EntityRef& dstRef, IMessage* msg) {
  msg->setSrcEntiity(ref_);
  msg->setDstEntiity(dstRef);
  dstRef.worker_->Send(msg);
}

}