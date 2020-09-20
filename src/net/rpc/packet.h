/*
 * Copyright (C) codedump
 */

#pragma once

#include <stdint.h>
#include <string>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>
#include "base/typedef.h"
#include "util/string.h"

using namespace std;
namespace gpb = ::google::protobuf;

namespace libraft {

// sizeof (magic_num) + sizeof (call_guid) + sizeof (method_id) + sizeof (size) = 21
static const uint32_t kMetaSize = 21;
// packet magic number
static const uint8_t kPacketMagicNum = 0x2B;

struct Packet {
public:	
	uint8_t magic;
	uint64_t guid;
	RpcMethodId method_id;
	uint32_t size;
	string content;
	bool recv_done;

	Packet() 
		: magic(0), 
			guid(0), 
			method_id(0), 
			size(0), 
			content(""),
			recv_done(false) {
	}
	
	Packet(uint64_t id, const gpb::MethodDescriptor *method, 
				 const gpb::Message *req) 
		: magic(kPacketMagicNum), guid(id), method_id(0) {
			req->SerializeToString(&content);
			if (method) {
				method_id = HashString(method->full_name());
			}
			size = content.size();
	}
};
};