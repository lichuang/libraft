/*
 * Copyright (C) lichuang
 */

#include "base/entity_type.h"
#include "base/server.h"
#include "base/worker_pool.h"
#include "net/tcp_acceptor.h"
#include "net/acceptor_entity.h"
#include "net/data_handler.h"

namespace libraft {
AcceptorEntity::AcceptorEntity(const ServiceOptions& options)
  : IEntity(kAcceptorEntity),
    acceptor_(nullptr),
    factory_(options.factory),
    address_(options.endpoint),
    after_listen_func_(options.after_listen_func) {   
  BindEntity(this);
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

  if (after_listen_func_) {
    after_listen_func_();
  }
}

void 
AcceptorEntity::Stop() {
}
};