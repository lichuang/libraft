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
#include "net/rpc/rpc_controller.h"
#include "net/rpc/rpc_session.h"

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

void onRpcRespose(WaitGroup* wait) {
  Info() << "after response";
  wait->Done();   
}

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
        
        Info() << "in EchoService::Echo:";

        response->set_echo_msg("world");
        string content;
        response->SerializeToString(&content);

        /*
        EchoResponse msg;
        msg.ParseFromString(content);
        string str = StringToHex(content);
        Info() << "in EchoService::Echo:" << str;
        */

        //wait.Done();
        done->Run();
    }
  };

  wait.Add(1);

  EchoServiceImpl echo_service_impl;
  ServiceOptions options;
  Endpoint ep = Endpoint("127.0.0.1", 22222);
  options.endpoint = ep;
  options.factory = new RpcSessionFactory();
  options.service = &echo_service_impl;
  options.after_listen_func = []() { 
    Info() << "begin accept new connection";
    wait.Done(); 
  };

  AddService(options);  

  // wait until acceptor bind
  wait.Wait();
  wait.Add(1);

  RpcChannelOptions channel_options;
  channel_options.server = ep;
  RpcController controller;
  
  EchoResponse response;

  channel_options.after_bound_func = [&] (RpcChannel* channel) {        
    EchoRequest request;

    request.set_msg("hello");
    EchoService_Stub stub(channel);
    stub.Echo(&controller, &request, &response, 
      gpb::NewCallback(&::onRpcRespose, &wait));    
  };

  RpcChannel* channel = CreateRpcChannel(channel_options);

  wait.Wait();
  DestroyRpcChannel(channel);
}

int main(int argc, char* argv[]) {
  testing::AddGlobalTestEnvironment(new RpcTestEnvironment);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}