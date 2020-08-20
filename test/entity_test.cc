/*
 * Copyright (C) codedump
 */

#include <gtest/gtest.h>
#include <string>
#include <iostream>
#include "base/entity.h"
#include "base/message.h"
#include "base/worker.h"

using namespace libraft;
using namespace std;

TEST(EntityTest, send_msg) {
  static int kTestEntityMsgType = 10100;
  struct TestEntityMsg1;

  struct TestEntityMsg1: public IMessage {
  public:
    TestEntityMsg1(int *t)
      : IMessage(kTestEntityMsgType),
        num_(t) {
    }

    int* num_;
  };

  class TestEntity1 : public IEntity {
  public:
    void Handle(IMessage* m) {
      TestEntityMsg1* msg = (TestEntityMsg1*)m;
      *(msg->num_) = 101;
    }
  };

  int a = 10;

  Worker worker1("worker1");
  //Worker worker2("worker1");

  worker1.Start();
  TestEntity1 te1;
  worker1.AddEntity(&te1);

  IMessage *msg = new TestEntityMsg1(&a);
  te1.Send(msg);

  worker1.Stop();

  ASSERT_EQ(a, 101);
}

TEST(EntityTest, ask_msg) {
  static int kTestAskEntityMsgType = 10100;

  struct TestAskEntityMsg;
  struct TestAskRespEntityMsg;

  struct TestAskEntityMsg: public IMessage {
  public:
    TestAskEntityMsg(const string& str)
      : IMessage(kTestAskEntityMsgType),
        str_(str) {
    }

    string str_;
  };

  struct TestAskRespEntityMsg: public IMessage {
  public:
    TestAskRespEntityMsg(const string& str)
      : IMessage(kTestAskEntityMsgType, true),
        str_(string("hello ") + str) {
    }

    string str_;
  };

  class TestAskEntity : public IEntity {
  public:
    void Handle(IMessage* m) {
    }
  };

  class TestRespEntity : public IEntity {
  public:
    void Handle(IMessage* m) {      
      TestAskEntityMsg* msg = (TestAskEntityMsg*)m;
      TestAskRespEntityMsg* resp = new TestAskRespEntityMsg(msg->str_);
      msg->dstRef_.Response(resp, msg);      
    }
  };

  Worker worker1("worker1");
  Worker worker2("worker2");

  worker1.Start();
  TestAskEntity te1;
  worker1.AddEntity(&te1);

  worker2.Start();
  TestRespEntity te2;
  worker2.AddEntity(&te2);

  TestAskEntityMsg *msg = new TestAskEntityMsg("libraft");
  ASSERT_EQ(msg->str_, "libraft");

  te1.Ask(msg, [](const IMessage* m) {
    TestAskRespEntityMsg* respmsg = (TestAskRespEntityMsg*)m;
    ASSERT_EQ(respmsg->str_, string("hello ") + "libraft");
    std::cout << "!!!\n";
  });

  worker1.Stop();
  worker2.Stop();
}

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}