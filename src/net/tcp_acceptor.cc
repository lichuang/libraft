/*
 * Copyright (C) lichuang
 */

#include <gflags/gflags.h>
#include "base/worker_pool.h"
#include "base/log.h"
#include "base/server.h"
#include "net/data_handler.h"
#include "net/net.h"
#include "net/session_entity.h"
#include "net/tcp_acceptor.h"

namespace libraft {

DEFINE_int32(backlog, 1024, "tcp listen backlog");

TcpAcceptor::TcpAcceptor(IHandlerFactory* factory, const Endpoint& ep)
  : factory_(factory),
    fd_(-1),
    address_(ep),
    event_loop_(CurrentEventLoop()),
    event_(nullptr) {    
}

TcpAcceptor::~TcpAcceptor() {
  if (event_) {
    event_->DisableAllEvent();
    Close(fd_);
    delete event_;
  }
}

void 
TcpAcceptor::Listen() {
  Status err;

  fd_ = libraft::Listen(address_, FLAGS_backlog, &err);
  if (!err.Ok()) {
    Fatal() << "listen to " << address_.String() << " fail:" << err.String();
    return;
  }
  
  EventLoop* loop = CurrentEventLoop();
  event_ = new IOEvent(loop, fd_, this);
  event_->EnableRead();
  Info() << "listening to " << address_.String() << " ...";  
}
 
void 
TcpAcceptor::onRead(IOEvent*) {
  Endpoint ep;
  Status status;

  while (true) {
    int fd = Accept(fd_, &ep, &status);
    if (!status.Ok()) {
      if (!status.TryIOAgain()) {
        Error() << "accept new connection at " << address_.String() << " fail:" << status.String();
      }
      break;
    }
    IDataHandler *handler = factory_->NewHandler();
    SessionEntity* se = new SessionEntity(handler, ep, fd);
    BindEntity(se);
  }  
}

void 
TcpAcceptor::onWrite(IOEvent*) {
  // nothing to do
  Fatal() << "acceptor cannot has write event";
}

};