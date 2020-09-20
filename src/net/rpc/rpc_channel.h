/*
 * Copyright (C) lichuang
 */

#pragma once

#include <map>
#include <queue>
#include <google/protobuf/message.h>
#include <google/protobuf/service.h>
#include "base/typedef.h"
#include "net/data_handler.h"

namespace gpb = ::google::protobuf;

namespace libraft {

class Packet;
class PacketParser;
class RpcController;
class RpcService;
struct RequestContext;

// protobuf rpc connection channel data handler
class RpcChannel : public IDataHandler,
                   public gpb::RpcChannel::RpcChannel {
public:
  RpcChannel(Socket*);
        
  virtual ~RpcChannel();

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
  void pushRequestToQueue(
      const gpb::MethodDescriptor *method,
      RpcController *controller,
      const gpb::Message *request,
      gpb::Message *response,
      gpb::Closure *done);

  id_t allocateId() {
    return ++allocate_id_;
  }

  id_t Id() const {
    return id_;
  }

private:
  // rpc packet parser
  PacketParser *parser_;

  // packet buffer queue
  queue<Packet*> packet_queue_;

	typedef map<uint64_t, RequestContext*> RequestContextMap;
	RequestContextMap request_context_;

  id_t id_;
  id_t allocate_id_;
  RpcService *service_;
};

class RpcChannelFactory : public IHandlerFactory {
public:
  RpcChannelFactory(){}
  virtual ~RpcChannelFactory() {
  }

  virtual IDataHandler* NewHandler(Socket* socket) {
    return new RpcChannel(socket);
  }
};
};