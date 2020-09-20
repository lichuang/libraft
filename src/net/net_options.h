/*
 * Copyright (C) lichuang
 */

#pragma once

#include "net/endpoint.h"

namespace libraft {

// function type be called after server call listen()
typedef void (*AfterListenFunc)();
class IHandlerFactory;
class Service;

// service options
struct ServiceOptions {
  ServiceOptions() 
    : after_listen_func(nullptr),
      factory(nullptr),
      service(nullptr) {
  }

  // called after service call listen(),which in the service bound worker
  AfterListenFunc after_listen_func;

  // logic handler factory
  IHandlerFactory *factory;

  // service listen endpoint
  Endpoint endpoint;

  // service,if it is null,service entity will create a default service 
  // in ServiceEntity::initAfterBind
  Service *service;

  void operator=(const ServiceOptions& options) {
    this->after_listen_func = options.after_listen_func;
    this->factory = options.factory;
    this->endpoint = options.endpoint;
    this->service = options.service;
  }
};

// connector options
struct ConnectorOptions {
  ConnectorOptions() 
    : factory(nullptr) {
  }
  
  // logic handler factory
  IHandlerFactory *factory;

  // connect to endpoint
  Endpoint endpoint;
};
};