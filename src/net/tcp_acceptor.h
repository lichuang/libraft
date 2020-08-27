/*
 * Copyright (C) lichuang
 */

#pragma once

#include "base/event.h"
#include "base/status.h"
#include "base/typedef.h"
#include "net/endpoint.h"

namespace libraft {

class EventLoop;
class SessionFactory;

class TcpAcceptor : public IIOHandler  {
public:
  TcpAcceptor(SessionFactory* factory, const Endpoint& ep, EventLoop* loop);

  ~TcpAcceptor();

  Status Listen(const Endpoint& ep);

  virtual void onRead(IOEvent*);

  virtual void onWrite(IOEvent*);

private:
  // SessionFactory
  SessionFactory* factory_;

  fd_t fd_;

  Endpoint address_;

  EventLoop *event_loop_;

  Event* event_;
};
};