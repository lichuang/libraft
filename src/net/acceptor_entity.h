/*
 * Copyright (C) lichuang
 */

#pragma once

#include "base/entity.h"
#include "base/typedef.h"
#include "net/endpoint.h"

namespace libraft {

class EventLoop;
class IHandlerFactory;
class Socket;
class TcpAcceptor;

typedef void (*ListenFunc)();

// class for acceptor entity
class AcceptorEntity : public IEntity {
public:
  AcceptorEntity(IHandlerFactory *factory, const Endpoint&, ListenFunc func = nullptr);

  ~AcceptorEntity();

  void initAfterBind();
  
  void Stop();
protected:
  TcpAcceptor* acceptor_;
  IHandlerFactory *factory_;
  Endpoint address_;

  ListenFunc func_;
};
};