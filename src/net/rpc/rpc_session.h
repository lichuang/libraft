/*
 * Copyright (C) lichuang
 */

#pragma once

#include <map>
#include <queue>
#include <google/protobuf/message.h>
#include <google/protobuf/service.h>
#include "net/data_handler.h"

namespace gpb = ::google::protobuf;

namespace libraft {

class Packet;
class PacketParser;
class RpcController;
struct RequestContext;

class RpcSession : public IDataHandler,
                   public gpb::RpcChannel::RpcChannel {
public:
  RpcSession();
        
  virtual ~RpcSession();

	// gpb::RpcChannel::RpcChannel virtual method
  virtual void CallMethod(
      const gpb::MethodDescriptor *method,
      gpb::RpcController *controller,
      const gpb::Message *request,
      gpb::Message *response,
      gpb::Closure *done);
      
  virtual void onWrite();

  virtual void onRead();
  
  virtual void onConnect(const Status&);

  virtual void onError(const Status&);

  void SetService(RpcService *service) {
    service_ = service;
  }

private:
  // rpc packet parser
  PacketParser *parser_;

  queue<Packet*> packet_queue_;

	typedef map<uint64_t, RequestContext*> RequestContextMap;
	RequestContextMap request_context_;

  RpcService *service_;
};

class RpcSessionFactory {
public:
  RpcSessionFactory(){}
  virtual ~RpcSessionFactory() {
  }

  virtual IDataHandler* NewHandler() {
    return new RpcSession();
  }
};
};