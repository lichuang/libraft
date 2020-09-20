/*
 * Copyright (C) codedump
 */

#include "base/log.h"
#include "net/socket.h"
#include "net/rpc/packet_parser.h"

namespace libraft {

PacketParser::PacketParser(Socket *socket)
	: socket_(socket), 
		state_(RECV_HEADER_STATE) {
}

PacketParser::~PacketParser() {

}

bool 
PacketParser::RecvPacket() {
	ASSERT_NOT_EQ(state_, RECV_DONE);

	size_t read_size = 0;
	bool done = false;

	while (!done) {
		switch (state_) {
			case RECV_HEADER_STATE:
				read_size = socket_->ReadBufferSize();
				if (read_size < kMetaSize) {
					Error() << "rpc from " << socket_->String() << " buffer size error:" << read_size;
					return false;
				}

				Info() << "socket read buffer size: " << socket_->ReadBufferSize()
					<< ", kMetaSize: " << kMetaSize;
				
				packet_.recv_done = false;

				// read packet meta info
				socket_->Read(reinterpret_cast<char*>(&packet_.magic), sizeof(packet_.magic));
				socket_->Read(reinterpret_cast<char*>(&packet_.guid), sizeof(packet_.guid));
				socket_->Read(reinterpret_cast<char*>(&packet_.method_id), sizeof(packet_.method_id));
				socket_->Read(reinterpret_cast<char*>(&packet_.size), sizeof(packet_.size));

				// check packet magic number
				if (packet_.magic != kPacketMagicNum) {
          Error() << "client package error, magic_num: "
              << packet_.magic
              << " call_guid: " << packet_.guid
              << " method_id: " << packet_.method_id
              << " size: " << packet_.size
							<< " address: " << socket_->String();			
					return false;
				}

				// now recv packet content
				state_ = RECV_PACKET_STATE;
				break;
			case RECV_PACKET_STATE:
				read_size = packet_.size;
				Debug() << "packet content size: " << read_size
					<< ", current socket read buffer size: " << socket_->ReadBufferSize();
				if (read_size > socket_->ReadBufferSize()) {
					Error() << "rpc from " << socket_->String() << " packet size " << read_size << " error";
					return false;
				}

				// read packet content from socket
				packet_.content.resize(packet_.size);
				socket_->Read(reinterpret_cast<char*>(&packet_.content[0]), read_size);
				state_ = RECV_DONE;
				break;

			case RECV_DONE:
				// recv a packet done
				state_ = RECV_HEADER_STATE;
				packet_.recv_done = true;
				done = true;
				break;						
		}
	}

	return true;
}

void 
PacketParser::SendPacket(Packet* packet) {
	// serialize packet content into socket buffer
  socket_->Write(reinterpret_cast<const char*>(&packet->magic), sizeof(packet->magic));
  socket_->Write(reinterpret_cast<const char*>(&packet->guid), sizeof(packet->guid));
  socket_->Write(reinterpret_cast<const char*>(&packet->method_id),sizeof(packet->method_id));
  socket_->Write(reinterpret_cast<const char*>(&packet->size),sizeof(packet->size));
  socket_->Write(reinterpret_cast<const char*>(&packet->content[0]),packet->content.size());

  Debug() << "SendPacket, guid: " << packet->guid
      << ", method id: " << packet->method_id
			<< ", size: " << packet->size
      << ", content: " << packet->content
			<< ", write buffer size: " << socket_->WriteBufferSize();	

	// now free packet
	delete packet;
}
};  // namespace libraft