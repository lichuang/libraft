/*
 * Copyright (C) lichuang
 */

#include <gtest/gtest.h>
#include <math.h>
#include "libraft.h"
#include "base/util.h"
#include "core/raft.h"
#include "core/progress.h"
#include "core/read_only.h"
#include "core/node.h"
#include "storage/memory_storage.h"
#include "raft_test_util.h"

using namespace libraft;

vector<Message> msgs;

static void appendStep(raft *, const Message &msg) {
  msgs.push_back(Message(msg));
}

//TODO
TEST(nodeTests, TestNodeStep) {
}

// TODO
// Cancel and Stop should unblock Step()
TEST(nodeTests, TestNodeStepUnblock) {
}

// TestNodePropose ensures that node.Propose sends the given proposal to the underlying raft.
TEST(nodeTests, TestNodePropose) {
  msgs.clear();
  vector<ReadState*> readStates;
  
  MemoryStorage *s = new MemoryStorage(NULL);
  vector<uint64_t> peers;
  peers.push_back(1);
  raft *r = newTestRaft(1, peers, 10, 1, s);
  Config config; 
  NodeImpl *n = new NodeImpl(r, &config);

  readStates.push_back(new ReadState(1, "somedata"));
  r->readStates_ = readStates;
  
  n->Campaign();
  Ready *ready = n->get_ready();

  while (true) {
    EXPECT_EQ(ready->readStates, readStates);

    s->Append(ready->entries);

    if (ready->softState.leader == r->id_) {
      n->Advance();
      break;
    }
    n->Advance();
  }

  r->stateStepFunc_ = appendStep;
  string wrequestCtx = "somedata2";
  n->ReadIndex(wrequestCtx);

  EXPECT_EQ((int)msgs.size(), 1);
  EXPECT_EQ(msgs[0].type(), MsgReadIndex);
  EXPECT_EQ(msgs[0].entries(0).data(), wrequestCtx);

  delete n;
}

// TestNodeReadIndexToOldLeader ensures that raftpb.MsgReadIndex to old leader
// gets forwarded to the new leader and 'send' method does not attach its term.
TEST(nodeTests, TestNodeReadIndexToOldLeader) {
  vector<uint64_t> peers = {1,2,3};
  vector<stateMachine*> sts;
  vector<MemoryStorage*> storages;
  raft *a, *b, *c;

  {
    MemoryStorage *s = new MemoryStorage(NULL);

    a = newTestRaft(1, peers, 10, 1, s);
    storages.push_back(s);
    sts.push_back(new raftStateMachine(a));
  }
  {
    MemoryStorage *s = new MemoryStorage(NULL);

    b = newTestRaft(2, peers, 10, 1, s);
    storages.push_back(s);
    sts.push_back(new raftStateMachine(b));
  }
  {
    MemoryStorage *s = new MemoryStorage(NULL);

    c = newTestRaft(3, peers, 10, 1, s);
    storages.push_back(s);
    sts.push_back(new raftStateMachine(c));
  }
  
  network *net = newNetwork(sts);

  // elect a as leader
  {
    vector<Message> tmp_msgs = { initMessage(1,1,MsgHup) };
    net->send(&tmp_msgs);
  }

  EntryVec testEntries = {initEntry(0,0,"testdata")};

  // send readindex request to b(follower)
  b->step(initMessage(2,2,MsgReadIndex,&testEntries));

  // verify b(follower) forwards this message to r1(leader) with term not set
  EXPECT_EQ((int)b->outMsgs_.size(), 1);

  Message readIndexMsg1 = initMessage(2,1,MsgReadIndex, &testEntries);

  EXPECT_TRUE(isDeepEqualMessage(*b->outMsgs_[0], readIndexMsg1));

  // send readindex request to c(follower)
  c->step(initMessage(3,3,MsgReadIndex,&testEntries));

  // verify c(follower) forwards this message to r1(leader) with term not set
  EXPECT_EQ((int)c->outMsgs_.size(), 1);
  Message readIndexMsg2 = initMessage(3,1,MsgReadIndex, &testEntries);
  EXPECT_TRUE(isDeepEqualMessage(*c->outMsgs_[0], readIndexMsg2));

  // now elect c as leader
  {
    vector<Message> tmp_msgs = {initMessage(3,3,MsgHup)};
    net->send(&tmp_msgs);
  }

  // let a steps the two messages previously we got from b, c
  a->step(readIndexMsg1);
  a->step(readIndexMsg2);

  // verify a(follower) forwards these messages again to c(new leader)
  EXPECT_EQ((int)a->outMsgs_.size(), 2);

  Message readIndexMsg3 = initMessage(1,3,MsgReadIndex, &testEntries);
  EXPECT_TRUE(isDeepEqualMessage(*a->outMsgs_[0], readIndexMsg3));
  EXPECT_TRUE(isDeepEqualMessage(*a->outMsgs_[1], readIndexMsg3));

  delete net;
}

// TestNodeProposeConfig ensures that node.ProposeConfChange sends the given configuration proposal
// to the underlying raft.
TEST(nodeTests, TestNodeProposeConfig) {
  msgs.clear();

  MemoryStorage *s = new MemoryStorage(NULL);
  vector<uint64_t> peers = {1};
  raft *r = newTestRaft(1, peers, 10, 1, s);
  Config config;
  NodeImpl *n = new NodeImpl(r, &config);
  
  n->Campaign();
  Ready *ready = n->get_ready();

  while (true) {
    s->Append(ready->entries);

    // change the step function to appendStep until this raft becomes leader
    if (ready->softState.leader == r->id_) {
      r->stateStepFunc_ = appendStep;
      n->Advance();
      break;
    }
    n->Advance();
  }

  ConfChange cc;
  cc.set_type(ConfChangeAddNode);
  cc.set_nodeid(1);
  string ccdata;
  cc.SerializeToString(&ccdata);

  n->ProposeConfChange(cc);
  ready = n->get_ready();
  n->Stop();

  EXPECT_EQ((int)msgs.size(), 1);
  EXPECT_EQ(msgs[0].type(), MsgProp);
  EXPECT_EQ(msgs[0].entries(0).data(), ccdata);

  delete n;
}

// TestNodeProposeAddDuplicateNode ensures that two proposes to add the same node should
// not affect the later propose to add new node.
void applyReadyEntries(Ready* ready, EntryVec* readyEntries, MemoryStorage *s, NodeImpl *n) {
  if (ready == NULL) {
    n->Advance();
    return;
  }
  size_t i;
  s->Append(ready->entries);
  Ready *nready;
  for (i = 0; i < ready->entries.size(); ++i) {
    const Entry& entry = ready->entries[i];
    
    readyEntries->push_back(entry);
    if (entry.type() == EntryNormal || entry.type() == EntryConfChange) {
      ConfChange cc;
      cc.ParseFromString(entry.data());
      ConfState cs;
      n->ApplyConfChange(cc, &cs);
      nready = n->get_ready();
      //applyReadyEntries(nready, readyEntries, s, n);
    }
  }
  n->Advance();
}

TEST(nodeTests, TestNodeProposeAddDuplicateNode) {
  MemoryStorage *s = new MemoryStorage(NULL);
  vector<uint64_t> peers;
  peers.push_back(1);
  raft *r = newTestRaft(1, peers, 10, 1, s);
  Config config;
  NodeImpl *n = new NodeImpl(r, &config);

  EntryVec readyEntries;
  n->Campaign();
  Ready *ready = n->get_ready();
  applyReadyEntries(ready, &readyEntries, s, n);

  ConfChange cc1, cc2;
  string ccdata1, ccdata2;

  cc1.set_type(ConfChangeAddNode);
  cc1.set_nodeid(1);
  cc1.SerializeToString(&ccdata1);
  n->ProposeConfChange(cc1);
  ready = n->get_ready();
  applyReadyEntries(ready, &readyEntries, s, n);
  
  // try add the same node again
  n->ProposeConfChange(cc1);
  ready = n->get_ready();
  applyReadyEntries(ready, &readyEntries, s, n);

  // the new node join should be ok
  cc2.set_type(ConfChangeAddNode);
  cc2.set_nodeid(2);
  cc2.SerializeToString(&ccdata2);
  n->ProposeConfChange(cc2);
  ready = n->get_ready();
  applyReadyEntries(ready, &readyEntries, s, n);

  EXPECT_EQ((int)readyEntries.size(), 4);
  EXPECT_EQ(readyEntries[1].data(), ccdata1);
  EXPECT_EQ(readyEntries[3].data(), ccdata2);

  delete n;
}
