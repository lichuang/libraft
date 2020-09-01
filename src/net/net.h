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

int   Listen(const Endpoint&, int backlog, Status *err);
void  ConnectAsync(const Endpoint&, int fd, Status* err);

int   Accept(int listen_fd, Endpoint*, Status* err);

// recv data from socket, put it into buffer
int   Recv(Socket *, Buffer *buffer, Status* err);

// send data to socket from buffer
int   Send(Socket *, Buffer *buffer, Status* err);

void  Close(int fd);
void  GetEndpointByFd(int fd, Endpoint*);

int   TcpSocket(Status* err);

};