/*
 * Copyright (C) lichuang
 */

#include "base/entity_type.h"
#include "base/server.h"
#include "base/worker_pool.h"
#include "net/service.h"
#include "net/service_entity.h"
#include "net/data_handler.h"

namespace libraft {
ServiceEntity::ServiceEntity(const ServiceOptions& options)
  : IEntity(kServiceEntity),
    service_(nullptr),
    options_(options),
    factory_(options.factory),
    address_(options.endpoint),
    after_listen_func_(options.after_listen_func) {   
  BindEntity(this);
}

ServiceEntity::~ServiceEntity() {
  delete factory_;
  if (service_) {
    delete service_;
  }
}

void 
ServiceEntity::initAfterBind() {
  service_ = new IService(options_);
  service_->Listen();

  if (after_listen_func_) {
    after_listen_func_();
  }
}

void 
ServiceEntity::Stop() {
}
};