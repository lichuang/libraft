/*
 * Copyright (C) codedump
 */

#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdint.h>
#include <string>
#include "util/string.h"

using namespace std;

namespace libraft {

static const string kUnknownEndpoint = "unknown endpoint";

class Endpoint {
public:
	Endpoint(const string& addr, uint16_t port)
		: addr_(addr), port_(port), str_("") {
		str_ = StringPrintf("%s:%d", addr_.c_str(), port_);
	}

	Endpoint() 
		: addr_(""), port_(0), str_(kUnknownEndpoint) {
	}

  Endpoint(const Endpoint& ep) {
		*this = ep;
	}

  void operator=(const Endpoint& ep);
	void operator=(const struct sockaddr_in& address);

	bool operator==(const Endpoint& ep) const;
	bool operator<(const Endpoint& ep)const;

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

inline void 
Endpoint::operator=(const struct sockaddr_in& addr) {
	char ipstr[INET6_ADDRSTRLEN];

	// deal with both IPv4 and IPv6:
	if (addr.sin_family == AF_INET) {
			struct sockaddr_in *s = (struct sockaddr_in *)&addr;
			port_ = ntohs(s->sin_port);
			inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);
	} else { // AF_INET6
			struct sockaddr_in6 *s = (struct sockaddr_in6 *)&addr;
			port_ = ntohs(s->sin6_port);
			inet_ntop(AF_INET6, &s->sin6_addr, ipstr, sizeof ipstr);
	}
	addr_ = ipstr;
	str_ = StringPrintf("%s:%d", addr_.c_str(), port_);
}

inline bool 
Endpoint::operator==(const Endpoint& ep) const {
	return this->port_ == ep.port_ && this->str_ == ep.str_;
}

inline bool 
Endpoint::operator<(const Endpoint& ep) const {
	if (this->port_ != ep.port_) {
		return this->port_ < ep.port_;
	}
	return this->str_ < ep.str_;
}
};