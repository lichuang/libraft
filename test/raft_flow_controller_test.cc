/*
 * Copyright (C) lichuang
 */

#include <gtest/gtest.h>
#include <math.h>
#include "libraft.h"
#include "raft_test_util.h"
#include "base/default_logger.h"
#include "core/raft.h"
#include "base/util.h"
#include "core/progress.h"
#include "core/read_only.h"
#include "storage/memory_storage.h"

using namespace libraft;

// TestMsgAppFlowControlFull ensures:
// 1. msgApp can fill the sending window until full
// 2. when the window is full, no more msgApp can be sent.
TEST(raftFlowController, TestMsgAppFlowControlFull) {
  vector<uint64_t> peers = {1,2};
  Storage *s = new MemoryStorage(&kDefaultLogger);
  raft *r = newTestRaft(1, peers, 5, 1, s);
  r->becomeCandidate();
  r->becomeLeader();
  Progress *pr2 = r->progressMap_[2];

  // force the progress to be in replicate state
  pr2->becomeReplicate();
  // fill in the inflights window
  int i;
  for (i = 0; i < r->maxInfilght_; ++i) {
    {
      EntryVec entries = {initEntry(0,0,"somedata")};
      Message msg = initMessage(1,1,MsgProp,&entries);

      r->step(msg);
    }

    MessageVec msgs;
    r->readMessages(&msgs);
    EXPECT_EQ((int)msgs.size(), 1);
  }

  // ensure 1
  EXPECT_TRUE(pr2->inflights_.full());

  // ensure 2
  for (i = 0; i < 10; ++i) {
    {
      EntryVec entries = {initEntry(0,0,"somedata")};
      Message msg = initMessage(1,1,MsgProp,&entries);
      r->step(msg);
    }

    MessageVec msgs;
    r->readMessages(&msgs);
    EXPECT_EQ((int)msgs.size(), 0);
  }

  delete r;
}

// TestMsgAppFlowControlMoveForward ensures msgAppResp can move
// forward the sending window correctly:
// 1. valid msgAppResp.index moves the windows to pass all smaller or equal index.
// 2. out-of-dated msgAppResp has no effect on the sliding window.
TEST(raftFlowController, TestMsgAppFlowControlMoveForward) {
  vector<uint64_t> peers = {1,2};
  Storage *s = new MemoryStorage(&kDefaultLogger);
  raft *r = newTestRaft(1, peers, 5, 1, s);
  r->becomeCandidate();
  r->becomeLeader();
  Progress *pr2 = r->progressMap_[2];

  // force the progress to be in replicate state
  pr2->becomeReplicate();
  // fill in the inflights window
  int i;
  for (i = 0; i < r->maxInfilght_; ++i) {
    {
      EntryVec entries = {initEntry(0,0,"somedata")};
      Message msg = initMessage(1,1,MsgProp,&entries);
      r->step(msg);
    }

    MessageVec msgs;
    r->readMessages(&msgs);
  }

 	// 1 is noop, 2 is the first proposal we just sent.
	// so we start with 2.
  for (i = 2; i < r->maxInfilght_; ++i) {
    {
      Message msg = initMessage(2,1,MsgAppResp, NULL,i);

      r->step(msg);
    }

    MessageVec msgs;
    r->readMessages(&msgs);

    // fill in the inflights window again
    {
      EntryVec entries = {initEntry(0,0,"somedata")};
      Message msg = initMessage(1,1,MsgProp,&entries);
      r->step(msg);
    }
    r->readMessages(&msgs);
    EXPECT_EQ((int)msgs.size(), 1);

    // ensure 1
    EXPECT_TRUE(pr2->inflights_.full());

    // ensure 2
    int j;
    for (j = 0; j < i; ++j) {
      {
        Message msg = initMessage(2,1,MsgAppResp,NULL,j);
        r->step(msg);
      }

      EXPECT_TRUE(pr2->inflights_.full());
    }
  }

  delete r;
}

// TestMsgAppFlowControlRecvHeartbeat ensures a heartbeat response
// frees one slot if the window is full.
TEST(raftFlowController, TestMsgAppFlowControlRecvHeartbeat) {
  vector<uint64_t> peers = {1,2};
  Storage *s = new MemoryStorage(&kDefaultLogger);
  raft *r = newTestRaft(1, peers, 5, 1, s);
  r->becomeCandidate();
  r->becomeLeader();
  Progress *pr2 = r->progressMap_[2];

  // force the progress to be in replicate state
  pr2->becomeReplicate();
  // fill in the inflights window
  int i;
  for (i = 0; i < r->maxInfilght_; ++i) {
    {
      EntryVec entries = {initEntry(0,0,"somedata")};
      Message msg = initMessage(1,1,MsgProp,&entries);
      r->step(msg);
    }

    MessageVec msgs;
    r->readMessages(&msgs);
  }

  for (i = 1; i < 5; ++i) {
    EXPECT_TRUE(pr2->inflights_.full());

    // recv tt msgHeartbeatResp and expect one free slot
    int j;
    for (j = 0; j < i; ++j) {
      {
        Message msg = initMessage(2,1,MsgHeartbeatResp,NULL);
        r->step(msg);
      }
      MessageVec msgs;
      r->readMessages(&msgs);
      EXPECT_FALSE(pr2->inflights_.full());
    }

    // one slot
    {
      EntryVec entries = {initEntry(0,0,"somedata")};
      Message msg = initMessage(1,1,MsgProp,&entries);

      r->step(msg);
      MessageVec msgs;
      r->readMessages(&msgs);
      EXPECT_EQ((int)msgs.size(), 1);
    }

    // and just one slot
    for (j = 0; j < 10; ++j) {
      {
        EntryVec entries = {initEntry(0,0,"somedata")};
        Message msg = initMessage(1,1,MsgProp,&entries);
        r->step(msg);
      }
      MessageVec msgs;
      r->readMessages(&msgs);
      EXPECT_EQ((int)msgs.size(), 0);
    }
    
    // clear all pending messages.
    {
      Message msg = initMessage(2,1,MsgHeartbeatResp,NULL);
      
      r->step(msg);
    }
    MessageVec msgs;
    r->readMessages(&msgs);
  }

  delete r;
}
