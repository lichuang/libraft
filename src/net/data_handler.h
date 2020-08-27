/*
 * Copyright (C) lichuang
 */

#pragma once

namespace libraft {

// virtual interface for socket data handler
class IDataHandler {
public:
  virtual ~IDataHandler() {
  }

  virtual void onWrite() { 

  }

  virtual void onRead() { 

  }
  
  virtual void onConnect(int error) {

  }

  virtual void onError(int error) {

  }
};

};