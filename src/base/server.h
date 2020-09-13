/*
 * Copyright (C) lichuang
 */

#pragma once

#include "base/singleton.h"

namespace libraft {

struct ServerOptions {
  // init default server options
  ServerOptions();

  // server worker thread num
  int worker_num;
};

class WorkerPool;
class Logger;
class IEntity;

class Server {
  friend class Singleton<Server>;
  
public:
  void Bind(IEntity* en);
  void Start(const ServerOptions& options);
  Logger* logger() {
    return logger_;
  }

  ~Server();
private:
  Server();

  void doInit() {        
  }

private:
  WorkerPool *worker_pool_;
  Logger *logger_;
};

#define gServer libraft::Singleton<libraft::Server>::Instance()
#define gLogger gServer->logger()

extern void StartServer(const ServerOptions&);
extern void StopServer();

extern void BindEntity(IEntity*);

}