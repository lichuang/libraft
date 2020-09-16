/*
 * Copyright (C) lichuang
 */

#pragma once

#include <map>
#include "base/singleton.h"
#include "net/net_options.h"

using namespace std;

namespace libraft {

struct ServerOptions {
  // init default server options
  ServerOptions();

  // server worker thread num
  int worker_num;
};

class ServiceEntity;
class WorkerPool;
class Logger;
class IEntity;

class Server {
  friend class Singleton<Server>;
  
public:
  void AddService(const ServiceOptions&);
  void ConnectTo(const ConnectorOptions&);

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
  typedef map<Endpoint, ServiceEntity*> AcceptorEntityMap;
  AcceptorEntityMap service_entities_;
};

#define gServer libraft::Singleton<libraft::Server>::Instance()
#define gLogger gServer->logger()

extern void StartServer(const ServerOptions&);
extern void StopServer();

extern void BindEntity(IEntity*);
extern void AddService(const ServiceOptions&);
extern void ConnectTo(const ConnectorOptions&);
}