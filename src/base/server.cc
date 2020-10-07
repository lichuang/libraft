/*
 * Copyright (C) lichuang
 */

#include <gflags/gflags.h>
#include "base/log.h"
#include "base/logger.h"
#include "base/server.h"
#include "base/worker_extern.h"
#include "base/worker_pool.h"
#include "net/service_entity.h"
#include "net/data_handler.h"
#include "net/session_entity.h"
#include "net/socket.h"

namespace libraft {

DEFINE_int32(worker_num, 2, "default worker num");

ServerOptions::ServerOptions()
  : worker_num(FLAGS_worker_num) {

}

Server::Server()
  : worker_pool_(nullptr),
    logger_(nullptr) {
}

Server::~Server() {
  delete worker_pool_;
  delete logger_;
}

void 
Server::AddService(const ServiceOptions& options) {
  ASSERT(service_entities_.find(options.endpoint) == service_entities_.end()) << "service for " << options.endpoint.String() << " existed";
  ServiceEntity *ae = new ServiceEntity(options);
  ASSERT(ae != nullptr) << "create service for " << options.endpoint.String() << " FAIL";
  service_entities_[options.endpoint] = ae;
}

void 
Server::ConnectTo(const ConnectorOptions& options) {
  Socket* socket = CreateClientSocket(options.endpoint);
  new SessionEntity(options.factory->NewHandler(socket), options.endpoint);  
}

void 
Server::Bind(IEntity* en) {
  worker_pool_->Bind(en);
}

void 
Server::Start(const ServerOptions& options) {
  initMainWorker();
  logger_ = new Logger();
  worker_pool_ = new WorkerPool(options.worker_num);  
  FATAL_IF(worker_pool_ == nullptr) << "woker start fail";
  Info() << "Server started...";
}

void 
StartServer(const ServerOptions& option) {
  gServer->Start(option);
}

void 
StopServer() {
  delete gServer;
}

void 
BindEntity(IEntity* en) {
  gServer->Bind(en);
}

void 
AddService(const ServiceOptions& options) {
  gServer->AddService(options);
}

void 
ConnectTo(const ConnectorOptions& options) {
  gServer->ConnectTo(options);
}
}