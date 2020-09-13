/*
 * Copyright (C) lichuang
 */

#pragma once

#include "base/entity.h"
#include "base/typedef.h"
#include "net/net_options.h"

namespace libraft {

class EventLoop;
class IHandlerFactory;
class Socket;
class TcpAcceptor;

typedef void (*ListenFunc)();

// class for acceptor entity
class AcceptorEntity : public IEntity {
public:
  AcceptorEntity(const ServiceOptions&);

  ~AcceptorEntity();

  void initAfterBind();
  
  void Stop();
protected:
  TcpAcceptor* acceptor_;
  IHandlerFactory *factory_;
  Endpoint address_;

  AfterListenFunc after_listen_func_;
};
};