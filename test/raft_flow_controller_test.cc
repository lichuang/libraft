#include <gtest/gtest.h>
#include <math.h>
#include "libraft.h"
#include "base/default_logger.h"
#include "base/util.h"
#include "core/raft.h"
#include "core/progress.h"
#include "core/read_only.h"
#include "storage/memory_storage.h"
#include "raft_test_util.h"

// TestMsgAppFlowControlFull ensures:
// 1. msgApp can fill the sending window until full
// 2. when the window is full, no more msgApp can be sent.
TEST(raftFlowController, TestMsgAppFlowControlFull) {
  vector<uint64_t> peers;
  peers.push_back(1);
  peers.push_back(2);
  Storage *s = new MemoryStorage(&kDefaultLogger);
  raft *r = newTestRaft(1, peers, 5, 1, s);
  r->becomeCandidate();
  r->becomeLeader();
  Progress *pr2 = r->prs_[2];

  // force the progress to be in replicate state
  pr2->becomeReplicate();
  // fill in the inflights window
  int i;
  for (i = 0; i < r->maxInfilght_; ++i) {
    {
      Message msg;
      msg.set_from(1);
      msg.set_to(1);
      msg.set_type(MsgProp);
      Entry *entry = msg.add_entries();
      entry->set_data("somedata");

      r->step(msg);
    }

    vector<Message*> msgs;
    r->readMessages(&msgs);
    EXPECT_EQ((int)msgs.size(), 1);
  }

  // ensure 1
  EXPECT_TRUE(pr2->ins_.full());

  // ensure 2
  for (i = 0; i < 10; ++i) {
    {
      Message msg;
      msg.set_from(1);
      msg.set_to(1);
      msg.set_type(MsgProp);
      Entry *entry = msg.add_entries();
      entry->set_data("somedata");

      r->step(msg);
    }

    vector<Message*> msgs;
    r->readMessages(&msgs);
    EXPECT_EQ((int)msgs.size(), 0);
  }
}

// TestMsgAppFlowControlMoveForward ensures msgAppResp can move
// forward the sending window correctly:
// 1. valid msgAppResp.index moves the windows to pass all smaller or equal index.
// 2. out-of-dated msgAppResp has no effect on the sliding window.
TEST(raftFlowController, TestMsgAppFlowControlMoveForward) {
  vector<uint64_t> peers;
  peers.push_back(1);
  peers.push_back(2);
  Storage *s = new MemoryStorage(&kDefaultLogger);
  raft *r = newTestRaft(1, peers, 5, 1, s);
  r->becomeCandidate();
  r->becomeLeader();
  Progress *pr2 = r->prs_[2];

  // force the progress to be in replicate state
  pr2->becomeReplicate();
  // fill in the inflights window
  int i;
  for (i = 0; i < r->maxInfilght_; ++i) {
    {
      Message msg;
      msg.set_from(1);
      msg.set_to(1);
      msg.set_type(MsgProp);
      Entry *entry = msg.add_entries();
      entry->set_data("somedata");

      r->step(msg);
    }

    vector<Message*> msgs;
    r->readMessages(&msgs);
  }

 	// 1 is noop, 2 is the first proposal we just sent.
	// so we start with 2.
  for (i = 2; i < r->maxInfilght_; ++i) {
    {
      Message msg;
      msg.set_from(2);
      msg.set_to(1);
      msg.set_type(MsgAppResp);
      msg.set_index(i);

      r->step(msg);
    }

    vector<Message*> msgs;
    r->readMessages(&msgs);

    // fill in the inflights window again
    {
      Message msg;
      msg.set_from(1);
      msg.set_to(1);
      msg.set_type(MsgProp);
      Entry *entry = msg.add_entries();
      entry->set_data("somedata");

      r->step(msg);
    }
    r->readMessages(&msgs);
    EXPECT_EQ((int)msgs.size(), 1);

    // ensure 1
    EXPECT_TRUE(pr2->ins_.full());

    // ensure 2
    int j;
    for (j = 0; j < i; ++j) {
      {
        Message msg;
        msg.set_from(2);
        msg.set_to(1);
        msg.set_index(j);
        msg.set_type(MsgAppResp);

        r->step(msg);
      }

      EXPECT_TRUE(pr2->ins_.full());
    }
  }
}

// TestMsgAppFlowControlRecvHeartbeat ensures a heartbeat response
// frees one slot if the window is full.
TEST(raftFlowController, TestMsgAppFlowControlRecvHeartbeat) {
  vector<uint64_t> peers;
  peers.push_back(1);
  peers.push_back(2);
  Storage *s = new MemoryStorage(&kDefaultLogger);
  raft *r = newTestRaft(1, peers, 5, 1, s);
  r->becomeCandidate();
  r->becomeLeader();
  Progress *pr2 = r->prs_[2];

  // force the progress to be in replicate state
  pr2->becomeReplicate();
  // fill in the inflights window
  int i;
  for (i = 0; i < r->maxInfilght_; ++i) {
    {
      Message msg;
      msg.set_from(1);
      msg.set_to(1);
      msg.set_type(MsgProp);
      Entry *entry = msg.add_entries();
      entry->set_data("somedata");

      r->step(msg);
    }

    vector<Message*> msgs;
    r->readMessages(&msgs);
  }

  for (i = 1; i < 5; ++i) {
    EXPECT_TRUE(pr2->ins_.full());

    // recv tt msgHeartbeatResp and expect one free slot
    int j;
    for (j = 0; j < i; ++j) {
      {
        Message msg;
        msg.set_from(2);
        msg.set_to(1);
        msg.set_type(MsgHeartbeatResp);

        r->step(msg);
      }
      vector<Message*> msgs;
      r->readMessages(&msgs);
      EXPECT_FALSE(pr2->ins_.full());
    }

    // one slot
    {
      Message msg;
      msg.set_from(1);
      msg.set_to(1);
      msg.set_type(MsgProp);
      msg.add_entries()->set_data("somedata");

      r->step(msg);
      vector<Message*> msgs;
      r->readMessages(&msgs);
      EXPECT_EQ((int)msgs.size(), 1);
    }

    // and just one slot
    for (j = 0; j < 10; ++j) {
      {
        Message msg;
        msg.set_from(1);
        msg.set_to(1);
        msg.set_type(MsgProp);
        msg.add_entries()->set_data("somedata");

        r->step(msg);
      }
      vector<Message*> msgs;
      r->readMessages(&msgs);
      EXPECT_EQ((int)msgs.size(), 0);
    }
    
    // clear all pending messages.
    {
      Message msg;
      msg.set_from(2);
      msg.set_to(1);
      msg.set_type(MsgHeartbeatResp);

      r->step(msg);
    }
    vector<Message*> msgs;
    r->readMessages(&msgs);
  }
}
