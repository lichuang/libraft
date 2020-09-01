/*
 * Copyright (C) lichuang
 */

#include <gflags/gflags.h>
#include "base/bind_entity_msg.h"
#include "base/worker.h"
#include "base/worker_pool.h"
#include "net/endpoint.h"
#include "util/string.h"

namespace libraft {

DEFINE_int32(select_worker_algo, 1, "select worker algorithm,1:round robin,2:hash by address");

static const int kByRoundRobin = 1;
static const int kByHashAddress = 2;

WorkerPool::WorkerPool()
  : current_(0) {
}

WorkerPool::~WorkerPool() {
  int i;
  for (i = 0; i < (int)workers_.size(); ++i) {
    Worker* worker = workers_[i];
    worker->Stop();
    delete worker;
  }

  workers_.clear();
}

void
WorkerPool::start(int num) {
  int i;
  for (i = 0; i < num; ++i) {
    Worker* worker = new Worker(StringPrintf("worker_%d", i+1), kWorkThread);
    workers_.push_back(worker);
  }
}

Worker* 
WorkerPool::next(IEntity* ) {
  return workers_[(current_++) % workers_.size()];
}

void 
WorkerPool::Bind(IEntity* en) {
  Worker *worker = next(en);
  worker->SendtoWorker(new bindEntityMsg(en));
  worker->AddEntity(en);
  // send bind message to worker
}

void 
CreateWorkerPool(int num) {
  gWorkerPool->start(num);
}
};