/*
 * Copyright (C) lichuang
 */

#pragma once

#include <map>
#include <queue>
#include <mutex>
#include <google/protobuf/message.h>
#include <google/protobuf/service.h>
#include "base/typedef.h"
#include "net/data_handler.h"

using namespace std;
namespace gpb = ::google::protobuf;

namespace libraft {

class IMessage;
class Packet;
class PacketParser;
class RpcController;
class RpcService;
struct RequestContext;

struct responseContext;

// protobuf rpc connection channel data handler
class RpcSession : public IDataHandler,
                   public gpb::RpcChannel::RpcChannel {
public:
  // called by Service::onRead
  RpcSession(Socket*);        

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

  virtual void onBeAccepted(Service* service) {
    service_ = (RpcService*)service;
  }

private:
  id_t allocateId() {
    return ++allocate_id_;
  }

  id_t Id() const {
    return id_;
  }

  void runService(const Packet& packet, uint64_t channel_guid);
  void onResponse(responseContext*);

private:
  // rpc packet parser
  PacketParser *parser_;

	typedef map<uint64_t, RequestContext*> RequestContextMap;
	RequestContextMap request_context_;

  id_t id_;
  id_t allocate_id_;
  RpcService *service_;
};

class RpcSessionFactory : public IHandlerFactory {
public:
  RpcSessionFactory(){}
  virtual ~RpcSessionFactory() {
  }

  virtual IDataHandler* NewHandler(Socket* socket) {
    return new RpcSession(socket);
  }
};
};