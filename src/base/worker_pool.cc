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

WorkerPool::WorkerPool(int num)
  : current_(0) {
  int i;
  for (i = 0; i < num; ++i) {
    Worker* worker = new Worker(StringPrintf("worker_%d", i+1), kWorkThread);
    workers_.push_back(worker);
  }    
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

Worker* 
WorkerPool::next(IEntity* ) {
  return workers_[(current_++) % workers_.size()];
}

void 
WorkerPool::Bind(IEntity* en) {
  // send bind message to worker
  Worker *worker = next(en);
  worker->SendtoWorker(new bindEntityMsg(en));  
}

};