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
class Service;

typedef void (*ListenFunc)();

// class for acceptor entity
class ServiceEntity : public IEntity {
public:
  ServiceEntity(const ServiceOptions&);

  ~ServiceEntity();

  void initAfterBind();
  
  void Stop();

protected:
  Service* service_;
  ServiceOptions options_;
  IHandlerFactory *factory_;
  Endpoint address_;

  AfterListenFunc after_listen_func_;
};
};