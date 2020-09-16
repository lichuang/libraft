/*
 * Copyright (C) lichuang
 */

#pragma once

#include "net/data_handler.h"

namespace libraft {
// internal prorobuf-based rpc protocol, including:meta(fixed size) + body(variable size)
// meta = magic(4 bytes) + protocol version(4 byte) + cmd(4 bytes) + body length(4 bytes)

// rpc state machine status
enum RpcStatus {
  IDLE          = 0,
  READING_META  = 1,
  READING_BODY  = 2,
};

struct Meta {
  uint32_t magic;
  uint32_t version;
  uint32_t cmd;
  uint32_t length;

  Meta()
    : magic(0)
    , version(0)
    , cmd(0)
    , length(0) {        
  }
};

class RpcHandler : public IDataHandler {
public:
  RpcHandler();
        
  virtual ~RpcHandler();

  virtual void onWrite();

  virtual void onRead(Buffer*);
  
  virtual void onConnect(const Status&);

  virtual void onError(const Status&);

private:
  // protocol state machine status
  int status_;

  // protocol meta data
  Meta meta_;
};

class RpcHandlerFactory {
public:
  RpcHandlerFactory(){}
  virtual ~RpcHandlerFactory() {
  }

  virtual IDataHandler* NewHandler() {
    return new RpcHandler();
  }
};
};