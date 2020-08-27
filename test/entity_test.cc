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
#include "base/worker.h"
#include "core/log.h"

using namespace libraft;
using namespace std;

TEST(EntityTest, send_msg) {
  static int kTestSendEntityMsgType = 10100;
  static int test_num = 10101;
  static WaitGroup wait;
  struct TestEntityMsg1;

  struct TestSendEntityMsg: public IMessage {
  public:
    TestSendEntityMsg(int *t)
      : IMessage(kTestSendEntityMsgType),
        num_(t) {
    }

    int* num_;
  };

  class TestEntity : public IEntity {
  public:
    TestEntity(Worker* w) : IEntity(w) {
    }
    
    void Handle(IMessage* m) {
      TestSendEntityMsg* msg = (TestSendEntityMsg*)m;
      *(msg->num_) = test_num;
      wait.Done();
    }
  };

  int a = 10;
  wait.Add(1);

  Worker worker1("worker1");

  TestEntity te1(&worker1);

  IMessage *msg = new TestSendEntityMsg(&a);
  Sendto(te1.Ref(), msg);

  wait.Wait();
  worker1.Stop();

  ASSERT_EQ(a, test_num);
}

TEST(EntityTest, ask_msg) {
  static int kTestAskEntityMsgType = 10100;
  static WaitGroup wg;

  struct TestAskEntityMsg;
  struct TestAskRespEntityMsg;

  wg.Add(1);

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
    TestAskEntity(Worker *w) : IEntity(w) {

    }
    void Handle(IMessage* m) {

    }
  };

  class TestRespEntity : public IEntity {
  public:
    TestRespEntity(Worker *w) : IEntity(w) {
      
    }

    void Handle(IMessage* m) {      
      TestAskEntityMsg* msg = (TestAskEntityMsg*)m;
      TestAskRespEntityMsg* resp = new TestAskRespEntityMsg(msg->str_);
      msg->srcRef_.Response(resp, msg);   
      Debug() << "send response";         
    }
  };

  Worker worker1("worker1");
  Worker worker2("worker2");

  TestAskEntity te1(&worker1);

  TestRespEntity te2(&worker2);

  TestAskEntityMsg *msg = new TestAskEntityMsg("libraft");
  ASSERT_EQ(msg->str_, "libraft");

  te1.Ask(te2.Ref(), msg, [](const IMessage* m) {
    TestAskRespEntityMsg* respmsg = (TestAskRespEntityMsg*)m;
    ASSERT_EQ(respmsg->str_, string("hello ") + "libraft"); 
    wg.Done();   
  });

  wg.Wait();

  worker1.Stop();
  worker2.Stop();
}

TEST(EntityTest, timer) {
  static WaitGroup wg;
  wg.Add(1);

  class TestTimerEntity : public IEntity {
  public:
    TestTimerEntity(Worker* w) : IEntity(w) {
    }
    
    void onTimeout(ITimerEvent*) {
      wg.Done();
      cout << "ontimeout\n";
    }
  };

  Worker worker1("worker1");

  TestTimerEntity te1(&worker1);

  worker1.RunOnce(&te1, Duration(kSecond));

  wg.Wait();
  worker1.Stop();
}

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}