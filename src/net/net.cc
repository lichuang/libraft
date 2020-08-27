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
#include "core/log.h"
#include "util/string.h"

namespace libraft {

static int createListenSocket(int*);
static int setNonBlocking(int fd);

static int
createListenSocket(int* err) {
  int fd, on;

  on = 1;
  if ((fd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
    Error() << "create socket fail:" << strerror(errno);
    *err = errno;
    return -1;
  }

  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1) {
    Error() << "setsockopt fail:" << strerror(errno);
    *err = errno;
    return -1;
  }

  return fd;
}

int
setNonBlocking(int fd) {
  int flags;

  if ((flags = fcntl(fd, F_GETFL)) == -1) {
    return errno;
  }
  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    return errno;
  }

  return kOK;
}

int
Listen(const Endpoint& endpoint, int backlog, int *err) {
  int                 fd;
  struct sockaddr_in  sa;
  int                 ret;

  *err = kOK;
  fd = createListenSocket(err);
  if (*err != kOK) {
    return kError;
  }

  ret = setNonBlocking(fd);
  if (ret != kOK) {
    goto error;
  }

  memset(&sa,0,sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_port = htons(static_cast<uint16_t>(endpoint.Port()));

  if (inet_aton(endpoint.Address().c_str(), &sa.sin_addr) == 0) {
    *err = errno;
    Error() << "inet_aton fail:" << strerror(errno);
    goto error;
  }

  if (bind(fd, reinterpret_cast<struct sockaddr*>(&sa), sizeof(sa)) < 0) {
    *err = errno;
    Error() << "bind fail:" << strerror(errno);
    goto error;
  }

  if (listen(fd, backlog) == -1) {
    *err = errno;
    Error() << "listen to " <<  endpoint.String() << " fail:" << strerror(errno);
    goto error;
  }

  return fd;

error:
  close(fd);
  return kError;
}

int
Accept(int listen_fd, int* err) {
  struct sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);
  int fd, ret;

  *err = kOK;
  while (true) {
    fd = ::accept(listen_fd, reinterpret_cast<struct sockaddr *>(&addr), &addrlen);
    if (fd == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        *err = errno;
        return kOK;
      }
      if (errno == EINTR) {
        continue;
      }
      *err = errno;
      return kError;
    }
    
    ret = setNonBlocking(fd);
    if (ret != kOK) {
      return kError;
    }
    break;
  }
  return fd;
}

int   
ConnectAsync(const Endpoint& remote, int fd) {
  int ret, err;

  sockaddr_in addr;
  socklen_t addr_len = sizeof(addr);
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr(remote.Address().c_str());
  addr.sin_port = htons(remote.Port());

  do {
    gErrno = 0;
    ret = ::connect(fd, reinterpret_cast<struct sockaddr *>(&addr), addr_len);
  } while (ret == -1 && gErrno == EINTR);

  if (ret == -1) {
    err = gErrno;
    if (err == EINPROGRESS) {
      return kOK;
    }

    Error() << "connect to " << remote.String() << " failed: "
      << StrError(gErrno);
    return gErrno;
  }

  return kOK;
}

int
Recv(Socket *socket, Buffer *buffer, int *err) {
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
  *err = 0;

  while(true) {
    nbytes = ::read(fd, buffer->WritePosition(), buffer->WritableBytes());
    if (nbytes > 0) {
      buffer->WriteAdvance(nbytes);
      ret += static_cast<int>(nbytes);
    } else if (nbytes < 0) {
      if (IsIOTryAgain(errno)) {
        // there is nothing in the tcp stack,return and wait for the next in event
        *err = gErrno;
        break;        
      } else if (errno == EINTR) {
        continue;
      } else {
        // something wrong has occoured
        *err = gErrno;
        Error() << "recv from " << socket->String() 
          << " failed: " << strerror(gErrno);
        return kError;
      }
    } else {
      // socket has been closed
      Error() << "socket from " << socket->String() 
        << " has been closed: " << strerror(gErrno);      
      *err = gErrno;
      return kError;
    }
  };

  return ret;
}

int
Send(Socket *socket, Buffer *buffer, int *err) {
  ssize_t nbytes;
  int ret;
  int fd = socket->fd();

  nbytes = 0;
  ret = 0;
  *err = 0;
  while (true) {
    if (buffer->ReadableBytes() == 0)  {
      // there is nothing in user-space stack to send
      break;
    }
    nbytes = ::write(fd, buffer->ReadPosition(), buffer->ReadableBytes());

    if (nbytes > 0) {
      buffer->ReadAdvance(nbytes);
      ret += static_cast<int>(nbytes);
    } else if (nbytes < 0) {
      if (errno == EINTR) {
        continue;
      } else if (IsIOTryAgain(errno)) {
        *err = gErrno;
        break;
      } else {
        *err = gErrno;
        Error() << "send to " << socket->String() 
          << " failed: " << strerror(gErrno);        
        return kError;
      }
    } else {
      // connection has been closed
      *err = gErrno;
      Error() << "socket from " << socket->String() 
        << " has been closed: " << strerror(gErrno);       
      return kError;
    }
  }

  return ret;
}

void  
GetEndpointByFd(int fd, Endpoint* endpoint) { 
  sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);
  if (::getpeername(fd, reinterpret_cast<struct sockaddr*>(&addr), &addrlen) < 0) {
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
TcpSocket() {
  int fd = socket(PF_INET, SOCK_STREAM, 0);
  setNonBlocking(fd);
  return fd;
}
};