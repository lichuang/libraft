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


int main(int argc, char* argv[]) {
  testing::AddGlobalTestEnvironment(new RpcTestEnvironment);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}