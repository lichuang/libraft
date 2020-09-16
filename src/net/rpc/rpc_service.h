/*
 * Copyright (C) lichuang
 */

#pragma once

#include <map>
#include <google/protobuf/service.h>
#include "base/typedef.h"
#include "net/net_options.h"
#include "net/service.h"

using namespace std;
namespace gpb = ::google::protobuf;

namespace libraft {
class RpcMeta;

// rpc service options
struct RpcServiceOptions : public ServiceOptions {
  RpcServiceOptions() : ServiceOptions() {

  }

  gpb::Service* service;
};

class RpcService : public IService {
  RpcService(const ServiceOptions&);

  ~RpcService();

private:
  void Register(gpb::Service*);

private:
	typedef map<MethodId, RpcMeta*> MethodMetaMap;
	MethodMetaMap method_map_;
};
};