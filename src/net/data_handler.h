/*
 * Copyright (C) lichuang
 */

#pragma once

#include <atomic>
#include "base/status.h"
#include "net/socket.h"

namespace libraft {

class Buffer;
class SessionEntity;
class Socket;

// virtual interface for socket data handler
class IDataHandler {
  friend class SessionEntity;

public:
  IDataHandler(Socket* socket)
    : socket_(socket),
      entity_(nullptr),
      bound_(ATOMIC_FLAG_INIT) {}
        
  virtual ~IDataHandler() {
    delete socket_;
  }

  void SetEntity(SessionEntity *entity) {
    entity_ = entity;
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
  bool isBound() const {
    return bound_.load();
  }
  
  virtual void onBound() {
    bound_.store(true);
  }
    
protected:
  Socket* socket_;
  SessionEntity *entity_;

  // whether or not has bound to a worker
  std::atomic<bool> bound_;
};

class IHandlerFactory {
public:
  IHandlerFactory(){}
  virtual ~IHandlerFactory() {
  }

  virtual IDataHandler* NewHandler(Socket*) = 0;
};
};