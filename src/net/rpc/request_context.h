/*
 * Copyright (C) lichuang
 */

#pragma once

#include <google/protobuf/message.h>
#include <google/protobuf/service.h>

namespace gpb = ::google::protobuf;

namespace libraft {

class RpcController;

struct RequestContext {
  RpcController* controller;
  gpb::Message *response;
  gpb::Closure *done;

  RequestContext(
      RpcController* c,
      gpb::Message* resp,
      gpb::Closure* func)
    : controller(c), response(resp), done(func) {
  }

  ~RequestContext() {
    if (done) {
      delete done;
      done = NULL;
    }
  }

  void Run() {
    gpb::Closure* cb = done;
    done = NULL;
    if (cb) {
      cb->Run();
    }
  }
};

};