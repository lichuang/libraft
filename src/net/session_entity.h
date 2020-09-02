/*
 * Copyright (C) lichuang
 */

#pragma once

#include "base/entity.h"
#include "base/typedef.h"
#include "net/endpoint.h"

namespace libraft {

class EventLoop;
class IDataHandler;
class Socket;

// class for server accepted entity
class SessionEntity : public IEntity {
public:
  SessionEntity(IDataHandler *handler, const Endpoint&, fd_t);
  SessionEntity(IDataHandler *handler, const Endpoint&);

  ~SessionEntity();

  void initAfterBind();
  
protected:
  Socket* socket_;
  IDataHandler *handler_;
  Endpoint address_;
  fd_t fd_;
};
};