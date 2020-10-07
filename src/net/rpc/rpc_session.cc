/*
 * Copyright (C) codedump
 */

#include "base/message.h"
#include "base/message_type.h"
#include "net/session_entity.h"
#include "net/socket.h"
#include "net/rpc/packet_parser.h"
#include "net/rpc/request_context.h"
#include "net/rpc/rpc_controller.h"
#include "net/rpc/rpc_meta.h"
#include "net/rpc/rpc_session.h"
#include "net/rpc/rpc_service.h"
#include "util/global_id.h"

namespace libraft {

struct responseContext {
  uint64_t channel_guid;
  uint64_t guid;
  const gpb::Message *request;
  gpb::Message *response;

  responseContext(uint64_t cguid, uint64_t g,
    const gpb::Message *req, gpb::Message *resp)
      : channel_guid(cguid), guid(g),
        request(req), response(resp) { 
  }

  ~responseContext() {
    delete request;
    delete response;
  }
};

RpcSession::RpcSession(Socket* socket)
	: IDataHandler(socket),
    parser_(new PacketParser(socket_)),
    id_(NewGlobalID()),
    allocate_id_(0),
    service_(nullptr) {	
}

RpcSession::~RpcSession() {
	delete parser_;
}

void 
RpcSession::onWrite() { 

}

void 
RpcSession::onRead() {
  Info() << "RpcSession::OnRead";
  while (parser_->RecvPacket()) {
    const Packet& packet = parser_->GetPacket();

    Debug() << "read: " << packet.guid
        << ", method id: " << packet.method_id
        << ", content: " << packet.content;

    if (packet.method_id != 0) {
      runService(packet, Id());
      continue;
    }

    if (request_context_.find(packet.guid) ==
        request_context_.end()) {
      Error() << "not found request context, request id: "
          << packet.guid
          << "method_id: " << packet.method_id;
      continue;
    }

    RequestContext* context = request_context_[packet.guid];

    bool ret = context->response->ParseFromString(packet.content);
    if (!ret) {
      Error() << "parse response fail: "
          << packet.content
          << " from " << socket_->String();
    }
    request_context_.erase(packet.guid);
    context->Run();
  }
}
  
void 
RpcSession::onConnect(const Status& status) {

}

void 
RpcSession::onError(const Status&) {
}

void 
RpcSession::runService(const Packet& packet, uint64_t channel_guid) {  
  RpcMeta* meta = service_->GetService(packet.method_id);
  if (meta == NULL) {
    Error() << "not found method for " << packet.method_id;
    return;
  }

  gpb::Message* request = meta->GetRequestPrototype().New();
  if (!request->ParseFromString(packet.content)) {
    Error() << "request ParseFromString fail: " << packet.guid;
    delete request;
    return;
  }
  gpb::Message* response = meta->GetResponsePrototype().New();

  meta->GetService()->CallMethod(
    &meta->GetMethodDescriptor(), 
    reinterpret_cast<gpb::RpcController*>(NULL),
    request, response,
    gpb::NewCallback(this, &RpcSession::onResponse,
                     new responseContext(channel_guid, packet.guid,
                     request, response)));
}

void 
RpcSession::CallMethod(
  const gpb::MethodDescriptor *method,
  gpb::RpcController *controller,
  const gpb::Message *request,
  gpb::Message *response,
  gpb::Closure *done) {
  if (socket_->IsClosed()) {
    return;
  }

  RpcController *rpc_controller = reinterpret_cast<RpcController*>(controller);

  uint64_t call_guid = allocateId();
  rpc_controller->Init(Id(), call_guid);

  Packet *packet = new Packet(call_guid, method, request);
  Debug() << "write to socket: " << call_guid << " : " << request->DebugString();

  request_context_[call_guid] = new RequestContext(rpc_controller, response, done);
  parser_->SendPacket(packet);
}

void 
RpcSession::onResponse(responseContext* context) {
  gpb::Message* response = context->response;
  Packet *packet = new Packet(context->guid, NULL, response);
  parser_->SendPacket(packet);

  delete context;
}

};