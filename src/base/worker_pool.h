/*
 * Copyright (C) lichuang
 */

#pragma once

#include <vector>
#include "base/singleton.h"

using namespace std;

namespace libraft {

class IEntity;
class IMessage;
class Worker;
class Endpoint;

class WorkerPool {
  friend class Singleton<WorkerPool>;
  friend class Worker;
  friend void CreateWorkerPool(int);
public:
  ~WorkerPool();  

  // after accept a connection, bind it to a worker
  void Bind(IEntity* en);

  int WorkerNum() const {
    return workers_.size();
  }

private:
  WorkerPool();
  void start(int);
  void doInit() {}
  Worker* next(IEntity* en);  

private:
  vector<Worker*> workers_;
  int current_;
};

extern void CreateWorkerPool(int);
extern bool InMainThread();

#define gWorkerPool libraft::Singleton<libraft::WorkerPool>::Instance()
};