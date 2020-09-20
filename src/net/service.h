/*
 * Copyright (C) lichuang
 */

#pragma once

#include "base/event.h"
#include "base/status.h"
#include "base/typedef.h"
#include "net/endpoint.h"
#include "net/net_options.h"

namespace libraft {

class IOEvent;
class EventLoop;
class IHandlerFactory;

class Service : public IIOHandler  {
public:
  Service(const ServiceOptions&);

  virtual ~Service();

  void Listen();

  virtual void onRead(IOEvent*);

  virtual void onWrite(IOEvent*);

  string String() const;

private:
  IHandlerFactory* factory_;

  // listen socket
  fd_t fd_;

  // listen address
  Endpoint address_;

  EventLoop *event_loop_;

  // listen event
  IOEvent* event_;
};

class IServiceFactory {
public:
  Service* NewService();
};
};