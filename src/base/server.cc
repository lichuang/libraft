/*
 * Copyright (C) lichuang
 */

#include <gflags/gflags.h>
#include "base/log.h"
#include "base/logger.h"
#include "base/server.h"
#include "base/worker_extern.h"
#include "base/worker_pool.h"

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

void BindEntity(IEntity* en) {
  gServer->Bind(en);
}

}