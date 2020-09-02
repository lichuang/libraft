/*
 * Copyright (C) lichuang
 */

#include <error.h>
#include <string.h>
#include "net/data_handler.h"
#include "net/net.h"
#include "net/socket.h"

namespace libraft {

// create a server side accepted socket
Socket* CreateServerSocket(const Endpoint& local, IDataHandler *handler, EventLoop* loop, fd_t fd) {
  return new Socket(local, handler, loop, fd);
}

// create a client side connect to server socket
Socket* CreateClientSocket(const Endpoint& remote, IDataHandler* handler, EventLoop* loop) {
  return new Socket(remote, handler, loop);
}

// create a server side socket
Socket::Socket(const Endpoint& local, IDataHandler* handler, EventLoop* loop, fd_t fd)
  : fd_(fd),
    handler_(handler),
    event_loop_(loop),
    is_writable_(false),
    status_(kSocketConnected),
    server_side_(true),
    local_endpoint_(local) {  
  GetEndpointByFd(fd, &remote_endpoint_);
  event_ = new IOEvent(event_loop_, fd_, this);
}

// create a client side socket
Socket::Socket(const Endpoint& remote, IDataHandler* h, EventLoop* loop)
  : fd_(TcpSocket(NULL)),
    handler_(h),
    event_loop_(loop),
    is_writable_(false),
    status_(kSocketInit),
    server_side_(false),
    remote_endpoint_(remote) {
  Status err;
  ConnectAsync(remote_endpoint_, fd_, &err);
  event_ = new IOEvent(event_loop_, fd_, this);
}

Socket::~Socket() {
  delete event_;
}

void 
Socket::connect() {
  ASSERT(!server_side_);

  Info() << "try connect to " << String();

  if (status_ != kSocketInit) {
    return;
  }

  status_ = kSocketConnecting;
  Status err;
  
  ConnectAsync(remote_endpoint_, fd_, &err);
  
  if (!err.Ok()) {
    //poller_->Add(fd_, this);
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
  int n;
  bool flag = false;

  while (true) {
    n = Recv(this, &read_buf_, &err);
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
    } else if (n > 0 && !flag) {
      // has read data from socket
      flag = true;
    }
  }

  if (flag && handler_) {
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
      break;
    }

    n = Send(this, &write_buf_, &err);
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

/*
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
  //return Recv.Read(to, n);
  return 0;
}
*/
};