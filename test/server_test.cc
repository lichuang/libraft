/*
 * Copyright (C) codedump
 */

#include <gtest/gtest.h>
#include <string>
#include <iostream>
#include "base/entity.h"
#include "base/duration.h"
#include "base/message.h"
#include "base/wait.h"
#include "base/worker_extern.h"
#include "base/worker_pool.h"
#include "base/log.h"
#include "net/data_handler.h"
#include "net/acceptor_entity.h"
#include "net/session_entity.h"
#include "net/socket.h"

using namespace libraft;

TEST(ServerTest, echo) {
  static WaitGroup wait;
  static string str("hello");

  // echo server socket data handler
  class EchoServerHandler : public IDataHandler {
  public:
    EchoServerHandler() {}
          
    virtual ~EchoServerHandler() {
    }

    virtual void onWrite() { 
      Info() << "server on write";
    }

    virtual void onRead() { 
      Info() << "server on read";
      //EXPECT_EQ();
      wait.Done();
    }
    
    virtual void onConnect(const Status&) {

    }

    virtual void onError(const Status&) {

    }

  };

  class EchoServerHandlerFactory : public IHandlerFactory {
  public:
    EchoServerHandlerFactory(){}
    virtual ~EchoServerHandlerFactory() {
    }

    virtual IDataHandler* NewHandler() {
      return new EchoServerHandler();
    }
  };

  // echo client socket data handler
  class EchoClientHandler : public IDataHandler {
  public:
    EchoClientHandler() {}
          
    virtual ~EchoClientHandler() {
    }

    virtual void onWrite() { 
      Info() << "client on write";
    }

    virtual void onRead() { 
      Info() << "client on read";
    }
    
    virtual void onConnect(const Status& err) {
      Info() << "after connect:" << err.String();
      socket_->Write(str.c_str(), str.length());
    }

    virtual void onError(const Status&) {

    }

  };

  class EchoClientHandlerFactory : public IHandlerFactory  {
  public:
    EchoClientHandlerFactory(){}
    virtual ~EchoClientHandlerFactory() {
    }

    virtual IDataHandler* NewHandler() {
      return new EchoClientHandler();
    }
  };  

  wait.Add(1);
  
  Endpoint ep = Endpoint("127.0.0.1", 22222);

  CreateWorkerPool(2);
  // create acceptor entity
  AcceptorEntity *ae = new AcceptorEntity(new EchoServerHandlerFactory(), ep, []() { 
    Info() << "begin accept new connection";
    wait.Done(); 
  });

  // wait until acceptor bind
  wait.Wait();

  wait.Add(1);

  // create client entity
  EchoClientHandlerFactory *cf = new EchoClientHandlerFactory();
  SessionEntity ce(cf->NewHandler(), ep);
  gWorkerPool->Bind(&ce);
  
  wait.Wait();
  ae = ae;
}

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}