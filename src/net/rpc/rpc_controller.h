/*
 * Copyright (C) codedump
 */

#pragma once

#include <string>
#include <google/protobuf/service.h>

using namespace std;
namespace gpb = ::google::protobuf;

namespace libraft {

class RpcController : public gpb::RpcController {
public:
	RpcController()
		: call_guid_(0),
			channel_guid_(0),
			server_fail_reason_("") {
	}

	virtual ~RpcController() {
	}

	// Client-side methods
	virtual void Reset() { }

	virtual bool Failed() const {
		return false;
	}

	virtual std::string ErrorText() const {
		return "";
	}

	virtual void StartCancel() {
		/*
		if (channel_guid_ == 0) {
			return;
		}
		ProtobufChannel* channel = service_->GetChannelByGuid(channel_guid_);
		if (channel == NULL) {
			return;
		}

		channel->Cancel();
		*/
	}

	// Server-side methods
	virtual void SetFailed(const std::string& reason) {
		server_fail_reason_ = reason;
	}

	virtual bool IsCanceled() const { 
		return false;
	}

	virtual void NotifyOnCancel(gpb::Closure* callback) {

	}

	void Init(uint64_t channel_guid, uint64_t call_guid) {
    channel_guid_ = channel_guid;
    call_guid_ = call_guid;
  }

  uint64_t GetCallGuid() const {
    return call_guid_;
  }

  uint64_t GetChannelGuid() const {
    return channel_guid_;
  }

private:
	//ProtobufService* service_;
	uint64_t call_guid_;
	uint64_t channel_guid_;
	string server_fail_reason_;
};

};