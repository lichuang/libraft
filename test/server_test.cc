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
#include "base/log.h"
#include "net/data_handler.h"
#include "net/socket.h"
#include "base/server.h"

using namespace libraft;

class ServerTestEnvironment : public testing::Environment
{
public:
    virtual void SetUp()
    {
        std::cout << "server test SetUP" << std::endl;
        StartServer(ServerOptions());
    }
    virtual void TearDown()
    {
        std::cout << "server test TearDown" << std::endl;
    }
};

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
  
  // create acceptor entity
  Endpoint ep = Endpoint("127.0.0.1", 22222);
  ServiceOptions options;
  options.endpoint = ep;
  options.factory = new EchoServerHandlerFactory();
  options.after_listen_func = []() { 
    Info() << "begin accept new connection";
    wait.Done(); 
  };
  
  AddService(options);  

  // wait until acceptor bind
  wait.Wait();

  wait.Add(1);

  // create client entity
  ConnectorOptions connector_options;
  
  connector_options.factory = new EchoClientHandlerFactory();
  connector_options.endpoint = ep;
  ConnectTo(connector_options);

  wait.Wait();
}

int main(int argc, char* argv[]) {
  testing::AddGlobalTestEnvironment(new ServerTestEnvironment);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}