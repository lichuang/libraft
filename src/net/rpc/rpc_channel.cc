/*
 * Copyright (C) codedump
 */

#include "base/message.h"
#include "base/message_type.h"
#include "base/worker_extern.h"
#include "net/session_entity.h"
#include "net/socket.h"
#include "net/rpc/packet_parser.h"
#include "net/rpc/request_context.h"
#include "net/rpc/rpc_channel.h"
#include "net/rpc/rpc_controller.h"
#include "util/global_id.h"

namespace libraft {

struct RpcCallMethodMsg: public IMessage {
public:
  RpcCallMethodMsg()
    : IMessage(kRpcCallMethodMessage) {
  }
};

RpcChannel::RpcChannel(const Endpoint& server)
	: IDataHandler(CreateClientSocket(server)),
    //parser_(new PacketParser(socket_)),
    id_(NewGlobalID()),
    allocate_id_(0) {
  entity_ = new SessionEntity(this, server);
  parser_ = new PacketParser(socket_);
  Info() << "init channel with socket " << this << ":" << socket_;
  entity_->RegisterMessageHandler(kRpcCallMethodMessage, std::bind(&RpcChannel::handleCallMethodMessage, this, std::placeholders::_1));  
}

RpcChannel::~RpcChannel() {
  DestroySocket();
	delete parser_;
  parser_ = nullptr;
}

void 
RpcChannel::onBound() {
  Info() << "RpcChannel::onBound()";
  IDataHandler::onBound();
  handlerPacketQueue();
}

void 
RpcChannel::pushRequestToQueue(
  const gpb::MethodDescriptor *method,
  RpcController *controller,
  const gpb::Message *request,
  gpb::Message *response,
  gpb::Closure *done) {
  Info() << "pushRequestToQueue";

  std::lock_guard<std::mutex> lock(mutex_);

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
  Info() << "RpcChannel::OnRead: " << this << ":" << socket_;
  while (parser_->RecvPacket()) {
    const Packet& packet = parser_->GetPacket();

    Debug() << "read: " << packet.guid
        << ", method id: " << packet.method_id
        << ", content: " << packet.content;

    if (packet.method_id == 0) {
      //Error() << "receive request packet " << packet.method_id;
      //continue;
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
  Info() << "connect to " << socket_->RemoteString() << " result: " << status.String();

  if (status.Ok()) {
    handlerPacketQueue();
    return;
  }  
}

void
RpcChannel::handlerPacketQueue() {
  std::lock_guard<std::mutex> lock(mutex_);

  while (!packet_queue_.empty()) {
    parser_->SendPacket(packet_queue_.front());
    packet_queue_.pop();
  }
  return;
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
  if (socket_->IsClosed()) {
    return;
  }
  RpcController *rpc_controller = (RpcController *)controller;

  if (socket_->IsInit() || socket_->IsConnecting()) {
    Info() << "socket " << socket_->String();
    pushRequestToQueue(method, rpc_controller, request, response, done);
    return;
  }

  if (!isBound() || !entity_->InSameWorker()) {
    Info() << "socket " << socket_->String();
    pushRequestToQueue(method, rpc_controller, request, response, done);
    if (entity_) {
      RpcCallMethodMsg* msg = new RpcCallMethodMsg();
      entity_->Send(msg);
    }
    if (isBound()) {      
      handlerPacketQueue();
    }
    return;
  }

  // in the same thread, call method directly
  doCallMethod(method, controller, request, response, done);
}

void 
RpcChannel::handleCallMethodMessage(IMessage* msg) {
  RpcCallMethodMsg* call_msg = (RpcCallMethodMsg*)msg;
  (void)call_msg;
  handlerPacketQueue();
  return;
}

void 
RpcChannel::doCallMethod(
  const gpb::MethodDescriptor *method,
  gpb::RpcController *controller,
  const gpb::Message *request,
  gpb::Message *response,
  gpb::Closure *done) {
  ASSERT(socket_->IsConnected()) << socket_->String() << " has not connected";
  RpcController *rpc_controller = reinterpret_cast<RpcController*>(controller);

  uint64_t call_guid = allocateId();
  rpc_controller->Init(Id(), call_guid);

  Packet *packet = new Packet(call_guid, method, request);
  Debug() << "write to socket: " << call_guid << " : " << request->DebugString();

  request_context_[call_guid] = new RequestContext(rpc_controller, response, done);
  parser_->SendPacket(packet);
}

RpcChannel* 
CreateRpcChannel(const Endpoint& server) {
  return new RpcChannel(server);
}

void
DestroyRpcChannel(RpcChannel* channel) {
  DestroyEntity(channel->entity_);
}

}
