/*
 * Copyright (C) lichuang
 */

#include "base/entity_type.h"
#include "base/worker_pool.h"
#include "net/tcp_acceptor.h"
#include "net/acceptor_entity.h"
#include "net/data_handler.h"

namespace libraft {
AcceptorEntity::AcceptorEntity(IHandlerFactory *factory, const Endpoint& ep, ListenFunc func)
  : IEntity(kAcceptorEntity),
    acceptor_(nullptr),
    factory_(factory),
    address_(ep),
    func_(func) {   
  gWorkerPool->Bind(this);
}

AcceptorEntity::~AcceptorEntity() {
  delete factory_;
  if (acceptor_) {
    delete acceptor_;
  }
}

void 
AcceptorEntity::initAfterBind() {
  acceptor_ = new TcpAcceptor(factory_, address_);
  acceptor_->Listen();

  if (func_) {
    func_();
  }
}

void 
AcceptorEntity::Stop() {
}
};