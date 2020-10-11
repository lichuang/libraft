/*
 * Copyright (C) lichuang
 */

#pragma once

#include <functional>
#include <google/protobuf/service.h>
#include "net/endpoint.h"

namespace gpb = ::google::protobuf;

namespace libraft {

// function type be called after server call listen()
typedef std::function<void ()> AfterListenFunc;

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

  // protobuf service(optional)
  // if not null, will create a RpcService
  gpb::Service* service;

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