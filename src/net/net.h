/*
 * Copyright (C) lichuang
 */

#pragma once

#include <sys/types.h>
#include <sys/socket.h>
#include <string>
#include "base/status.h"
#include "net/endpoint.h"

using namespace std;

namespace libraft {

class Buffer;
class Socket;

int   Listen(const Endpoint&, int backlog, int *err);
int   ConnectAsync(const Endpoint&, int);

int   Accept(int listen_fd, int*);

// recv data from socket, put it into buffer,
// if some error occurred, save error in err param
int   Recv(Socket *, Buffer *buffer, int *err);

// send data to socket from buffer,
// if some error occurred, save error in err param
int   Send(Socket *, Buffer *buffer, int *err);

void  Close(int fd);
void  GetEndpointByFd(int fd, Endpoint*);

int   TcpSocket();

};