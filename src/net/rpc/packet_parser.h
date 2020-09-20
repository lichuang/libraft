/*
 * Copyright (C) codedump
 */

#pragma once

#include "base/define.h"
#include "net/rpc/packet.h"

namespace libraft {

class Socket;

// PacketParser for RPC packet
class PacketParser {
public:
	PacketParser(Socket *);
	~PacketParser();

	// recv a packet from socket
	// if true, then can get the recvived packet from GetPacket
	bool RecvPacket();

	// send packet to socket
	void SendPacket(Packet* packet);

	const Packet& GetPacket() const { 
		return packet_;
	}

private:
	// rpc Packet recv statemachine states
	enum State {
		// receive packet header
		RECV_HEADER_STATE = 0,
		// receive packet content
		RECV_PACKET_STATE = 1,
		// receive done
		RECV_DONE         = 2,
	};

private:
	// corresponding socket
	Socket *socket_;
	// parser state
	State state_;
	// recv Packet buffer
	Packet packet_;

	DISALLOW_COPY_AND_ASSIGN(PacketParser);
};

};