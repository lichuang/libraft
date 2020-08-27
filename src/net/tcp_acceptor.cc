/*
 * Copyright (C) lichuang
 */

#include "net/tcp_acceptor.h"

namespace libraft {

TcpAcceptor::TcpAcceptor(SessionFactory* factory, const Endpoint& ep, EventLoop* loop)
  : factory_(factory),
    fd_(-1),
    address_(ep),
    event_loop_(loop) {
}

TcpAcceptor::~TcpAcceptor() {

}

Status 
TcpAcceptor::Listen(const Endpoint& ep) {

}
 
void 
TcpAcceptor::onRead(IOEvent*) {

}

void 
TcpAcceptor::onWrite(IOEvent*) {
  // nothing to do
}

};