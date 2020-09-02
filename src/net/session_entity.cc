/*
 * Copyright (C) lichuang
 */

#include "base/entity_type.h"
#include "base/worker_extern.h"
#include "net/session_entity.h"
#include "net/socket.h"

namespace libraft {
SessionEntity::SessionEntity(IDataHandler *handler, const Endpoint& ep, fd_t fd)
  : IEntity(kSessionEntity),
    socket_(nullptr),
    handler_(handler),
    address_(ep),
    fd_(fd) {
}

SessionEntity::SessionEntity(IDataHandler *handler, const Endpoint& ep)
  : IEntity(kSessionEntity),
    socket_(nullptr),
    handler_(handler),
    address_(ep),
    fd_(-1) {
}

SessionEntity::~SessionEntity() {
  delete socket_;
  delete handler_;
}

void 
SessionEntity::initAfterBind() {
  EventLoop* loop = CurrentEventLoop();
  if (fd_ != -1) {
    socket_ = CreateServerSocket(address_, handler_, loop, fd_);
  } else {
    socket_ = CreateClientSocket(address_, handler_, loop);
  }
}
};