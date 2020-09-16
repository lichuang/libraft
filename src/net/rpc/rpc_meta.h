/*
 * Copyright (C) codedump
 */

#pragma once

#include <google/protobuf/descriptor.h>
#include <google/protobuf/service.h>
#include <string>

using namespace std;

namespace libraft {

namespace gpb = ::google::protobuf;

// RPC Method meta info class
class RpcMeta {
public:
	RpcMeta(gpb::Service* service, const gpb::MethodDescriptor* desp)
		: service_(service), methodDesp_(desp) {}

	gpb::Service* GetService() {
  	return service_;
	}

	const gpb::MethodDescriptor& GetMethodDescriptor() const {
  	return *methodDesp_;
	}

	const gpb::Message& GetRequestPrototype() const {
  return service_->GetRequestPrototype(methodDesp_);
	}

	const gpb::Message& GetResponsePrototype() const {
  	return service_->GetResponsePrototype(methodDesp_);
	}

	const string GetMethodName() const {
		return methodDesp_->full_name();
	}

private:
	gpb::Service* service_;
	const gpb::MethodDescriptor* methodDesp_;
};

};