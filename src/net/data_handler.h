/*
 * Copyright (C) lichuang
 */

#pragma once
#include "base/status.h"

namespace libraft {

class Buffer;
class Socket;

// virtual interface for socket data handler
class IDataHandler {
public:
  IDataHandler()
    : socket_(NULL) {}
        
  virtual ~IDataHandler() {
  }

  void SetSocket(Socket* sk) {
    socket_ = sk;
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

  virtual IDataHandler* NewHandler() = 0;
};
};