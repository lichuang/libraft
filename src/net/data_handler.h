/*
 * Copyright (C) lichuang
 */

#pragma once
#include "base/status.h"
#include "net/socket.h"

namespace libraft {

class Buffer;
class Socket;

// virtual interface for socket data handler
class IDataHandler {
public:
  IDataHandler(Socket* socket)
    : socket_(socket) {}
        
  virtual ~IDataHandler() {
    delete socket_;
  }

  Socket* socket() {
    return socket_;
  }

  virtual void onWrite() { 

  }

  virtual void onRead() { 

  }
  
  virtual void onConnect(const Status&) {

  }

  virtual void onError(const Status&) {

  }

protected:
  Socket* socket_;
};

class IHandlerFactory {
public:
  IHandlerFactory(){}
  virtual ~IHandlerFactory() {
  }

  virtual IDataHandler* NewHandler(Socket*) = 0;
};
};