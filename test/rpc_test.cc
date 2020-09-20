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
#include "net/net_options.h"
#include "base/server.h"
#include "echo.pb.h"
#include "net/rpc/rpc_channel.h"

using namespace libraft;

class RpcTestEnvironment : public testing::Environment
{
public:
    virtual void SetUp() {
      StartServer(ServerOptions());
    }

    virtual void TearDown() {
      StopServer();
    }
};

TEST(RpcServerTest, echo) {
  static WaitGroup wait;

  class EchoServiceImpl : public EchoService {
  public:
    EchoServiceImpl() {};
    virtual ~EchoServiceImpl() {};
    virtual void Echo(google::protobuf::RpcController* cntl,
                      const EchoRequest* request,
                      EchoResponse* response,
                      google::protobuf::Closure* done) {
          
        response->set_echo_msg("world");
        string content;
        response->SerializeToString(&content);

        EchoResponse msg;
        msg.ParseFromString(content);
        string str = StringToHex(content);
        Info() << "in EchoService::Echo:" << str;

        done->Run();
    }
  };

  wait.Add(1);

  EchoServiceImpl echo_service_impl;
  ServiceOptions options;
  Endpoint ep = Endpoint("127.0.0.1", 22222);
  options.endpoint = ep;
  options.factory = new RpcChannelFactory();
  options.service = &echo_service_impl;
  options.after_listen_func = []() { 
    Info() << "begin accept new connection";
    wait.Done(); 
  };

  AddService(options);  

  // wait until acceptor bind
  wait.Wait();

  wait.Add(1);
}

int main(int argc, char* argv[]) {
  testing::AddGlobalTestEnvironment(new RpcTestEnvironment);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}