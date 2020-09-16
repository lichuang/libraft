/*
 * Copyright (C) lichuang
 */

#pragma once

namespace libraft {
class IHandlerFactory;

// AcceptorHandler handle client connect socket event
// when a client connected to the server, it create 
// a new data handler using IHandlerFactory
class AcceptorHandler {
public:
  AcceptorHandler(IHandlerFactory* factory)
    : factory_(factory) {
  }
  virtual ~AcceptorHandler() {
    delete factory_;
  }

  // OnAccept create a new session using IHandlerFactory
  virtual Session* onAccept(int) {}
  virtual void onError(int error) {}
  
protected:
  IHandlerFactory* factory_;  
};
};