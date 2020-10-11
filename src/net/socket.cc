/*
 * Copyright (C) lichuang
 */

#include <error.h>
#include <string.h>
#include "base/log.h"
#include "net/data_handler.h"
#include "net/net.h"
#include "net/socket.h"
#include "util/string.h"

namespace libraft {

// create a server side accepted socket
Socket* CreateServerSocket(const Endpoint& local, fd_t fd) {
  return new Socket(local, fd);
}

// create a client side connect to server socket
Socket* CreateClientSocket(const Endpoint& remote) {
  return new Socket(remote);
}

// create a server side socket
Socket::Socket(const Endpoint& local, fd_t fd)
  : fd_(fd),
    handler_(nullptr),
    event_loop_(nullptr),
    is_writable_(false),
    server_side_(true),
    local_endpoint_(local) {  
  GetRemoteEndpoint(fd, &remote_endpoint_);
  //desc_ = local_endpoint_.String();
  desc_ = StringPrintf("[%s->%s]", remote_endpoint_.String().c_str(), local_endpoint_.String().c_str());

  status_.store(kSocketConnected, std::memory_order_relaxed);
}

// create a client side socket
Socket::Socket(const Endpoint& remote)
  : fd_(TcpSocket(NULL)),
    handler_(nullptr),
    event_loop_(nullptr),
    is_writable_(false),
    server_side_(false),
    remote_endpoint_(remote),
    event_(nullptr) {
  status_.store(kSocketInit, std::memory_order_relaxed);
  desc_ = StringPrintf("[(init)->%s]", remote_endpoint_.String().c_str());
}

Socket::~Socket() {
  if (event_) {
    delete event_;
    event_ = nullptr;
  }
}

void
Socket::Init(IDataHandler* handler, EventLoop* loop) {
  ASSERT(handler_ == nullptr) << String() << " handler has been inited";
  handler_ = handler;
  event_loop_ = loop;

  if (server_side_) {
    event_ = new IOEvent(event_loop_, fd_, this);
    event_->EnableRead();
  } else {
    event_ = new IOEvent(event_loop_, fd_, this);
    event_->EnableRead();
    connect();
  }
}

void 
Socket::connect() {
  ASSERT(!server_side_);

  Info() << "try connect to " << remote_endpoint_.String();

  if (status_ != kSocketInit) {
    return;
  }

  status_ = kSocketConnecting;
  Status err;
  
  ConnectAsync(remote_endpoint_, fd_, &err);
  GetLocalEndpoint(fd_, &local_endpoint_);
  desc_ = StringPrintf("[%s->%s]", local_endpoint_.String().c_str(), remote_endpoint_.String().c_str());

  if (!err.Ok()) {
    event_->EnableWrite();
  } else {
    handler_->onConnect(err);
    status_ = kSocketConnected;
  }
}

void
Socket::close() {

}

void
Socket::onRead(IOEvent*) {
  Debug() << "socket " << RemoteString() << " in";
  if (status_ != kSocketConnected) {
    Error() << "reevice read event when unconnected";
    return;
  }
  Status err;

  while (true) {
    Recv(this, &read_buf_, &err);
    Info()  << "readable size:" << read_buf_.ReadableBytes();
    if (!err.Ok() && !err.TryIOAgain()) {
      if (handler_) {
        handler_->onError(err);
      }
      Error() << String() <<" recv error: " << err.String();
      Socket::close();
      return;
    } else if (err.TryIOAgain()) {
      break;      
    }
  }

  int bytes = (int)read_buf_.ReadableBytes();
  printf("ReadableBytes:%d\n", bytes);
  if (bytes > 0 && handler_) {
    // if read data from socket, call handler->onRead    
    handler_->onRead();
  }
}

void
Socket::onWrite(IOEvent*) {
  Info() << "socket " << String() << " out";
  size_t n;
  Status err;
  bool flag = false;

  if (status_ == kSocketConnecting) {
    status_ = kSocketConnected;
    handler_->onConnect(Status());
    Info() << String() << " connected to " << remote_endpoint_.String() << " success";
  }

  if (status_ != kSocketConnected) {
    Error() << "socket is closed, cannot write data" << status_;
    return;
  }

  while (true) {
    if (write_buf_.Empty()) {
      // when write buffer empty, clear the writeable flag
      is_writable_ = false;
      event_->DisableWrite();
      break;
    }

    n = Send(this, &write_buf_, &err);
    Info() << "send " << n << " bytes, err:" << err.String() << write_buf_.Empty();

    if (!err.Ok() && !err.TryIOAgain()) {
      if (handler_) {
        handler_->onError(err);
      }
      Socket::close();
      return;
    } else if (err.TryIOAgain()) {
        break;      
    } else if (n > 0 && !flag) {
      flag = true;
    }
  }

  if (flag && handler_) {
    // if write some data, call handler->onWrite
    handler_->onWrite();
  }
}

void
Socket::Write(const char* from, size_t n) {
  // write data to write buffer
  write_buf_.Write(from, n);
  if (!is_writable_) {
    is_writable_ = true;
    event_->EnableWrite();
  }
}

size_t
Socket::Read(char* to, size_t n) {
  return read_buf_.Read(to, n);
}

};