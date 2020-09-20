/*
 * Copyright (C) codedump
 */

#include "net/socket.h"
#include "net/rpc/packet_parser.h"
#include "net/rpc/request_context.h"
#include "net/rpc/rpc_channel.h"
#include "net/rpc/rpc_controller.h"
#include "util/global_id.h"

namespace libraft {

RpcChannel::RpcChannel(Socket* socket)
	: IDataHandler(socket),
    parser_(new PacketParser(socket_)),
    id_(NewGlobalID()),
    allocate_id_(0) {	
}

RpcChannel::~RpcChannel() {
	delete parser_;
}

void 
RpcChannel::pushRequestToQueue(
  const gpb::MethodDescriptor *method,
  RpcController *controller,
  const gpb::Message *request,
  gpb::Message *response,
  gpb::Closure *done) {
  Info() << "pushRequestToQueue";

  uint64_t call_guid = allocateId();
  controller->Init(Id(), call_guid);
  packet_queue_.push(new Packet(call_guid, method, request));

  request_context_[call_guid] = new RequestContext(controller, response, done);
}

void 
RpcChannel::onWrite() { 

}

void 
RpcChannel::onRead() {
  Info() << "RpcChannel::OnRead";
  while (parser_->RecvPacket()) {
    const Packet& packet = parser_->GetPacket();

    Debug() << "read: " << packet.guid
        << ", method id: " << packet.method_id
        << ", content: " << packet.content;

    if (packet.method_id != 0) {
      Error() << "receive request packet " << packet.method_id;
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
    if (!context) {
      return;
    }
    bool ret = context->response->ParseFromString(packet.content);

    request_context_.erase(packet.guid);
    if (!ret) {
      Error() << "parse response "
          << StringToHex(packet.content)
          << " from " << socket_->String() << " failed";
    } else {
      context->Run();
    }
    delete context;  
  }
}
  
void 
RpcChannel::onConnect(const Status& status) {
  if (status.Ok()) {
    while (!packet_queue_.empty()) {
      parser_->SendPacket(packet_queue_.front());
      packet_queue_.pop();
    }
    return;
  }

  //Error() << "connect to " << socket_->String() << " failed: " << error;
}

void 
RpcChannel::onError(const Status&) {
}

void 
RpcChannel::CallMethod(
  const gpb::MethodDescriptor *method,
  gpb::RpcController *controller,
  const gpb::Message *request,
  gpb::Message *response,
  gpb::Closure *done) {
  RpcController *rpc_controller = reinterpret_cast<RpcController*>(controller);

  if (socket_->IsClosed()) {
    return;
  }

  if (socket_->IsConnected()) {
    uint64_t call_guid = allocateId();
    rpc_controller->Init(Id(), call_guid);

    Packet *packet = new Packet(call_guid, method, request);
    Debug() << "write to socket: " << call_guid << " : " << request->DebugString();

    request_context_[call_guid] = new RequestContext(rpc_controller, response, done);
    parser_->SendPacket(packet);
    return;
  }

  if (socket_->IsInit()) {
    pushRequestToQueue(method, rpc_controller, request, response, done);
    //socket_->Connect();
  }

  if (socket_->IsConnecting()) {
    Debug() << "connecting";
    pushRequestToQueue(method, rpc_controller, request, response, done);
  }
}

};