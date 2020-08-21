/*
 * Copyright (C) lichuang
 */

#include "base/entity.h"
#include "base/message.h"
#include "base/worker.h"

namespace libraft {

void 
EntityRef::Send(IMessage* msg) {
  worker->Send(msg);
}

void 
EntityRef::Response(IMessage* msg, IMessage* srcMsg) {
  msg->responseFor(srcMsg);
  worker->Send(msg);
}

void 
IEntity::Ask(const EntityRef& dstRef, IMessage* msg, MessageResponseFn fn) {
  resp_fn_map_[msg->id_] = fn;
  msg->setDstEntiity(dstRef);
  msg->setSrcEntiity(ref_);
  dstRef.worker->Send(msg);  
}

void 
IEntity::HandleResponse(IMessage* msg) {
  MessageResponseMap::iterator iter = resp_fn_map_.find(msg->id_);
  if (iter == resp_fn_map_.end()) {
    return;
  }
  iter->second(msg);
  resp_fn_map_.erase(msg->id_);
}

void 
IEntity::Sendto(const EntityRef& dstRef, IMessage* msg) {
  msg->setSrcEntiity(ref_);
  msg->setDstEntiity(dstRef);
  dstRef.worker->Send(msg);
}

}