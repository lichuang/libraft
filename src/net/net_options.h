/*
 * Copyright (C) lichuang
 */

#pragma once

#include "net/endpoint.h"

namespace libraft {

// function type be called after server call listen()
typedef void (*AfterListenFunc)();
class IHandlerFactory;

// service options
struct ServiceOptions {
  ServiceOptions() 
    : after_listen_func(nullptr),
      factory(nullptr) {
  }

  AfterListenFunc after_listen_func;

  // logic handler factory
  IHandlerFactory *factory;

  // service listen endpoint
  Endpoint endpoint;

  void operator=(const ServiceOptions& options) {
    this->after_listen_func = options.after_listen_func;
    this->factory = options.factory;
    this->endpoint = options.endpoint;
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