/*
 * Copyright (C) codedump
 */

#pragma once

#include <stdint.h>
#include <string>
#include "util/string.h"

using namespace std;

namespace libraft {

class Endpoint {
public:
	Endpoint(const string& addr, uint16_t port)
		: addr_(addr), port_(port), str_("") {
		str_ = StringPrintf("%s:%d", addr_.c_str(), port_);
	}

	Endpoint() 
		: addr_(""), port_(0), str_("") {
	}

  Endpoint(const Endpoint& ep) {
		*this = ep;
	}

  void operator=(const Endpoint& ep);
	bool operator==(const Endpoint& ep);

	const string& Address() const {
		return addr_;
	}

	uint16_t Port() const {
		return port_;
	}
	
	const string& String() const {
 		return str_;
	}

private:
	string addr_;
	uint16_t port_;
	string str_;
};

inline void 
Endpoint::operator=(const Endpoint& ep) {
	this->addr_ = ep.addr_;
	this->port_ = ep.port_;
	this->str_ = ep.str_;
}

inline bool 
Endpoint::operator==(const Endpoint& ep) {
	return this->port_ == ep.port_ && this->str_ == ep.str_;
}

};