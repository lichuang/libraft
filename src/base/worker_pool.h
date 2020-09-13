/*
 * Copyright (C) lichuang
 */

#pragma once

#include <vector>
#include "base/worker_extern.h"

using namespace std;

namespace libraft {

class IEntity;
class IMessage;
class Worker;
class Endpoint;
class Server;

class WorkerPool {
  friend class Worker;
  friend class Server;
public:
  ~WorkerPool();  

  // after accept a connection, bind it to a worker
  void Bind(IEntity* en);

  int WorkerNum() const {
    return workers_.size();
  }

private:
  WorkerPool(int num);
  void doInit() {}
  Worker* next(IEntity* en);  

private:
  vector<Worker*> workers_;
  int current_;
};

};