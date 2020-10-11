/*
 * Copyright (C) lichuang
 */

#include <sys/eventfd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "base/buffer.h"
#include "base/status.h"
#include "net/net.h"
#include "net/socket.h"
#include "base/log.h"
#include "util/string.h"

namespace libraft {

static int createListenSocket(Status*);
static int setNonBlocking(int fd, Status *err);

static int
createListenSocket(Status* err) {
  int fd, on;

  on = 1;
  if ((fd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
    *err = Status(errno,strerror(errno));
    return -1;
  }

  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1) {
    *err = Status(errno,strerror(errno));
    return -1;
  }

  return fd;
}

int
setNonBlocking(int fd, Status *err) {
  int flags;

  if ((flags = fcntl(fd, F_GETFL)) == -1) {
    *err = Status(errno,strerror(errno));
    
    return kError;
  }
  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    *err = Status(errno,strerror(errno));
    return kError;
  }

  return kOK;
}

int
Listen(const Endpoint& endpoint, int backlog, Status *err) {
  int                 fd;
  struct sockaddr_in  sa;

  *err = kOK;
  fd = createListenSocket(err);
  if (!err->Ok()) {
    return kError;
  }

  setNonBlocking(fd, err);
  if (!err->Ok()) {
    goto error;
  }

  ::memset(&sa,0,sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_port = ::htons(static_cast<uint16_t>(endpoint.Port()));

  if (::inet_aton(endpoint.Address().c_str(), &sa.sin_addr) == 0) {
    *err = Status(errno,strerror(errno));
    goto error;
  }

  if (::bind(fd, reinterpret_cast<struct sockaddr*>(&sa), sizeof(sa)) < 0) {
    *err = Status(errno,strerror(errno));
    goto error;
  }

  if (::listen(fd, backlog) == -1) {
    *err = Status(errno,strerror(errno));
  }

  return fd;

error:
  close(fd);
  return kError;
}

int
Accept(int listen_fd, Endpoint *ep, Status* err) {
  struct sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);
  int fd;

  *err = kOK;
  while (true) {
    fd = ::accept(listen_fd, reinterpret_cast<struct sockaddr *>(&addr), &addrlen);
    if (fd == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        *err = kTryIOAgain;
        return kOK;
      }
      if (errno == EINTR) {
        continue;
      }
      *err = Status(errno, strerror(errno));;
      return kError;
    }
    
    setNonBlocking(fd, err);
    if (!err->Ok()) {
      return kError;
    }
    *ep = addr;
    break;
  }
  return fd;
}

void   
ConnectAsync(const Endpoint& remote, int fd, Status* err) {
  int ret;

  sockaddr_in addr;
  socklen_t addr_len = sizeof(addr);
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr(remote.Address().c_str());
  addr.sin_port = htons(remote.Port());

  do {
    ret = ::connect(fd, reinterpret_cast<struct sockaddr *>(&addr), addr_len);
  } while (ret == -1 && errno == EINTR);

  if (ret == -1) {    
    if (errno == EINPROGRESS) {
      return;
    }

    *err = Status(errno, strerror(errno));
  } else {
    *err = kOK;
  }
}

int   
Recv(Socket *socket, Buffer *buffer, Status* err) {
  ssize_t nbytes;
  int ret;
  int fd = socket->fd();

	/*
	 * recv data from tcp socket until:
	 *  1) some error occur or
	 *  2) received data less then required size
	 *    (means there is no data in the tcp stack buffer)
	 */
  nbytes = 0;
  ret = 0;

  while(true) {
    nbytes = ::read(fd, buffer->WritePosition(), buffer->WritableBytes());
    if (nbytes > 0) {
      buffer->WriteAdvance(nbytes);
      ret += static_cast<int>(nbytes);
    } else if (nbytes < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // there is nothing in the tcp stack,return and wait for the next in event
        *err = kTryIOAgain;
        break;        
      } else if (errno == EINTR) {
        continue;
      } else {
        // something wrong has occoured
        *err = Status(kError, strerror(errno));
      }
    } else {
      // socket has been closed  
      *err = Status(kError, strerror(errno));
    }
  };

  return ret;
}

int
Send(Socket *socket, Buffer *buffer, Status* err) {
  ssize_t nbytes;
  int ret;
  int fd = socket->fd();

  nbytes = 0;
  ret = 0;
  while (buffer->ReadableBytes() > 0) {
    nbytes = ::write(fd, buffer->ReadPosition(), buffer->ReadableBytes());

    if (nbytes > 0) {
      buffer->ReadAdvance(nbytes);
      ret += static_cast<int>(nbytes);
    } else if (nbytes < 0) {
      if (errno == EINTR) {
        continue;
      } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
        *err = kTryIOAgain;
        break;
      } else {
        *err = Status(errno, strerror(errno));
      }
    } else {
      *err = Status(errno, strerror(errno));
    }
  }

  return ret;
}

void  
GetRemoteEndpoint(int fd, Endpoint* endpoint) { 
  sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);
  if (::getpeername(fd, reinterpret_cast<struct sockaddr*>(&addr), &addrlen) < 0) {
    Error() << "get peer addr fail, fd: " << fd << " error: " << errno;
    return;
  }

  *endpoint = Endpoint(inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
}

void
GetLocalEndpoint(int fd, Endpoint* endpoint) { 
  sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);
  if (::getsockname(fd, reinterpret_cast<struct sockaddr*>(&addr), &addrlen) < 0) {
    Error() << "get peer addr fail, fd: " << fd << " error: " << errno;
    return;
  }

  *endpoint = Endpoint(inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
}

void
Close(int fd) {
  close(fd);
}

int
TcpSocket(Status *err) {
  int fd = socket(PF_INET, SOCK_STREAM, 0);
  setNonBlocking(fd, err);
  return fd;
}
};