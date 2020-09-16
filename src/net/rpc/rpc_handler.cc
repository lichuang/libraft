/*
 * Copyright (C) lichuang
 */

#include <stdint.h>
#include "net/rpc_handler.h"

namespace libraft {

RpcHandler::RpcHandler()
  : status_(IDLE) {

}
        
RpcHandler::~RpcHandler() {

}

void 
RpcHandler::onWrite() {
  // nothing to do
}

void 
RpcHandler::onRead(Buffer*) {

}
  
void 
RpcHandler::onConnect(const Status&) {

}

void 
RpcHandler::onError(const Status&) {

}

};