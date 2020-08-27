/*
 * Copyright (C) lichuang
 */

#include <error.h>
#include <string.h>
#include "net/data_handler.h"
#include "net/net.h"
#include "net/socket.h"

namespace libraft {

// create a server side socket
Socket* CreateServerSocket(const Endpoint& local, EventLoop* loop, IDataHandler *handler, fd_t fd) {
  return new Socket(local, loop, handler, fd);
}

// create a client side socket
Socket* CreateClientSocket(const Endpoint& remote,EventLoop* loop, IDataHandler* handler) {
  return new Socket(remote, loop, handler);
}

// create a server side socket
Socket::Socket(const Endpoint& local, EventLoop* loop, IDataHandler* handler, fd_t fd)
  : fd_(fd),
    handler_(handler),
    event_loop_(loop),
    is_writable_(false),
    status_(kSocketConnected),
    server_side_(true),
    local_endpoint_(local) {  
  GetEndpointByFd(fd, &remote_endpoint_);
  event_ = new IOEvent(event_loop_, fd_, this);
  event_->EnableRead();  
}

// create a client side socket
Socket::Socket(const Endpoint& remote, EventLoop* loop, IDataHandler* h)
  : fd_(TcpSocket()),
    handler_(h),
    event_loop_(loop),
    is_writable_(false),
    status_(kSocketInit),
    server_side_(false),
    remote_endpoint_(remote) { 
  event_ = new IOEvent(event_loop_, fd_, this);
  event_->EnableRead();        
  connect(); 
}

Socket::~Socket() {

}

void 
Socket::connect() {
  ASSERT(!server_side_);

  Info() << "try connect to " << String();

  if (status_ != kSocketInit) {
    return;
  }

  status_ = kSocketConnecting;
  int err = ConnectAsync(remote_endpoint_, fd_);
  
  if (err == kOK) {
    //poller_->Add(fd_, this);
  } else {
    handler_->onConnect(err);
    status_ = kSocketConnected;
  }
}

void
Socket::close() {

}

/*
void
Socket::In() {
  Debug() << "socket " << String() << " in";
  if (status_ != SOCKET_CONNECTED) {
    return;
  }
  int err;

  while (true) {
    Recv(this, &read_list_, &err);
    Info() << "read_list size:" << read_list_.TotalSize()
      << ", readable size:" << read_list_.ReadableSize();
    if (err != kOK && !IsIOTryAgain(err)) {
      if (handler_) {
        handler_->onError(err);
      }
      Error() << String() <<" recv error: " << strerror(err);
      Socket::close();
      return;
    } else {
      if (IsIOTryAgain(err)) {
        break;
      }
    }
  }

  if (!read_list_.Empty() && handler_) {
    // if read buffer is not empty, call handler->onRead
    handler_->onRead();
  }
}

void
Socket::Out() {
  Info() << "socket " << endpoint_.String() << " out";
  int err, n;

  if (status_ == SOCKET_CONNECTING) {
    status_ = SOCKET_CONNECTED;
    handler_->onConnect(kOK);
    Info() << "connected to " << endpoint_.String() << " success";
  }

  if (status_ != SOCKET_CONNECTED) {
    Error() << "socket is closed, cannot write data" << status_;
    return;
  }

  while (true) {
    if (write_list_.Empty()) {
      // when write buffer empty, clear the writeable flag
      is_writable_ = false;
      break;
    }

    n = Send(this, &write_list_, &err);
    if (err != kOK && !IsIOTryAgain(err)) {
      if (handler_) {
        handler_->onError(err);
      }
      Socket::close();
      return;
    } else {
      if (IsIOTryAgain(err)) {
        break;
      }
    }
  }

  if (n > 0 && handler_) {
    // if write some data, call handler->onWrite
    handler_->onWrite();
  }
}

void
Socket::Timeout() {
}
*/

void
Socket::Write(const char* from, size_t n) {
  // write data to write buffer
  write_buf_.Write(from, n);
  if (!is_writable_) {
    // when write buffer is not empty, set the writeable flag
    is_writable_ = true;
    // mark this handler writeable
    //poller_->MarkWriteable(handle_);
  }
}

size_t
Socket::Read(char* to, size_t n) {
  // read data info read buffer
  //int   Recv(Socket *, Buffer *buffer, int *err);
  return Recv.Read(to, n);
}

};