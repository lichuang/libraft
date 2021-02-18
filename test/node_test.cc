#include <gtest/gtest.h>
#include <math.h>
#include "libraft.h"
#include "base/util.h"
#include "base/default_logger.h"
#include "core/raft.h"
#include "core/progress.h"
#include "storage/memory_storage.h"
#include "core/read_only.h"
#include "core/node.h"
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
  NodeImpl *n = new NodeImpl();
  n->logger_ = &kDefaultLogger;
  Storage *s = new MemoryStorage(&kDefaultLogger);
  vector<uint64_t> peers;
  peers.push_back(1);
  raft *r = newTestRaft(1, peers, 10, 1, s);
  n->raft_ = r;

  readStates.push_back(new ReadState(1, "somedata"));
  r->readStates_ = readStates;

  Ready *ready;
  n->Campaign(&ready);

  while (true) {
    EXPECT_EQ(ready->readStates, readStates);

    s->Append(ready->entries);

    if (ready->softState.leader == r->id_) {
      n->Advance();
      break;
    }
    n->Advance();
  }

  r->stateStep = appendStep;
  string wrequestCtx = "somedata2";
  n->ReadIndex(wrequestCtx, &ready);

  EXPECT_EQ((int)msgs.size(), 1);
  EXPECT_EQ(msgs[0].type(), MsgReadIndex);
  EXPECT_EQ(msgs[0].entries(0).data(), wrequestCtx);
}

// TestNodeReadIndexToOldLeader ensures that raftpb.MsgReadIndex to old leader
// gets forwarded to the new leader and 'send' method does not attach its term.
TEST(nodeTests, TestNodeReadIndexToOldLeader) {
  vector<uint64_t> peers;
  vector<stateMachine*> sts;
  peers.push_back(1);
  peers.push_back(2);
  peers.push_back(3);

  raft *a, *b, *c;
  {
    MemoryStorage *s = new MemoryStorage(&kDefaultLogger);

    a = newTestRaft(1, peers, 10, 1, s);
    sts.push_back(new raftStateMachine(a));
  }
  {
    MemoryStorage *s = new MemoryStorage(&kDefaultLogger);

    b = newTestRaft(2, peers, 10, 1, s);
    sts.push_back(new raftStateMachine(b));
  }
  {
    MemoryStorage *s = new MemoryStorage(&kDefaultLogger);

    c = newTestRaft(3, peers, 10, 1, s);
    sts.push_back(new raftStateMachine(c));
  }
  
  network *net = newNetwork(sts);

  // elect a as leader
  {
    vector<Message> tmp_msgs;
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgHup);
    tmp_msgs.push_back(msg);
    net->send(&tmp_msgs);
  }

  EntryVec testEntries;
  {
    Entry entry;
    entry.set_data("testdata");
    testEntries.push_back(entry);
  }

  // send readindex request to b(follower)
  {
    Message msg;
    msg.set_from(2);
    msg.set_to(2);
    msg.set_type(MsgReadIndex);
    *(msg.add_entries()) = testEntries[0];
    b->step(msg);
  }

  // verify b(follower) forwards this message to r1(leader) with term not set
  EXPECT_EQ((int)b->msgs_.size(), 1);

  Message readIndexMsg1;
  readIndexMsg1.set_from(2);
  readIndexMsg1.set_to(1);
  readIndexMsg1.set_type(MsgReadIndex);
  *(readIndexMsg1.add_entries()) = testEntries[0];

  EXPECT_TRUE(isDeepEqualMessage(*b->msgs_[0], readIndexMsg1));

  // send readindex request to c(follower)
  {
    Message msg;
    msg.set_from(3);
    msg.set_to(3);
    msg.set_type(MsgReadIndex);
    *(msg.add_entries()) = testEntries[0];
    c->step(msg);
  }

  // verify c(follower) forwards this message to r1(leader) with term not set
  EXPECT_EQ((int)c->msgs_.size(), 1);
  Message readIndexMsg2;
  readIndexMsg2.set_from(3);
  readIndexMsg2.set_to(1);
  readIndexMsg2.set_type(MsgReadIndex);
  *(readIndexMsg2.add_entries()) = testEntries[0];
  EXPECT_TRUE(isDeepEqualMessage(*c->msgs_[0], readIndexMsg2));

  // now elect c as leader
  {
    vector<Message> tmp_msgs;
    Message msg;
    msg.set_from(3);
    msg.set_to(3);
    msg.set_type(MsgHup);
    tmp_msgs.push_back(msg);
    net->send(&tmp_msgs);
  }

  // let a steps the two messages previously we got from b, c
  a->step(readIndexMsg1);
  a->step(readIndexMsg2);

  // verify a(follower) forwards these messages again to c(new leader)
  EXPECT_EQ((int)a->msgs_.size(), 2);

  Message readIndexMsg3;
  readIndexMsg3.set_from(1);
  readIndexMsg3.set_to(3);
  readIndexMsg3.set_type(MsgReadIndex);
  *(readIndexMsg3.add_entries()) = testEntries[0];
  EXPECT_TRUE(isDeepEqualMessage(*a->msgs_[0], readIndexMsg3));
  EXPECT_TRUE(isDeepEqualMessage(*a->msgs_[1], readIndexMsg3));
}

// TestNodeProposeConfig ensures that node.ProposeConfChange sends the given configuration proposal
// to the underlying raft.
TEST(nodeTests, TestNodeProposeConfig) {
  msgs.clear();

  NodeImpl *n = new NodeImpl();
  n->logger_ = &kDefaultLogger;
  Storage *s = new MemoryStorage(&kDefaultLogger);
  vector<uint64_t> peers;
  peers.push_back(1);
  raft *r = newTestRaft(1, peers, 10, 1, s);
  n->raft_ = r;

  Ready *ready;
  n->Campaign(&ready);

  while (true) {
    s->Append(ready->entries);

    // change the step function to appendStep until this raft becomes leader
    if (ready->softState.leader == r->id_) {
      r->stateStep = appendStep;
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

  n->ProposeConfChange(cc, &ready);
  n->Stop();

  EXPECT_EQ((int)msgs.size(), 1);
  EXPECT_EQ(msgs[0].type(), MsgProp);
  EXPECT_EQ(msgs[0].entries(0).data(), ccdata);
}

// TestNodeProposeAddDuplicateNode ensures that two proposes to add the same node should
// not affect the later propose to add new node.
void applyReadyEntries(Ready* ready, EntryVec* readyEntries, Storage *s, NodeImpl *n) {
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
      n->ApplyConfChange(cc, &cs, &nready);
      //applyReadyEntries(nready, readyEntries, s, n);
    }
  }
  n->Advance();
}

TEST(nodeTests, TestNodeProposeAddDuplicateNode) {
  NodeImpl *n = new NodeImpl();
  n->logger_ = &kDefaultLogger;
  Storage *s = new MemoryStorage(&kDefaultLogger);
  vector<uint64_t> peers;
  peers.push_back(1);
  raft *r = newTestRaft(1, peers, 10, 1, s);
  n->raft_ = r;

  Ready *ready;
  EntryVec readyEntries;
  n->Campaign(&ready);
  applyReadyEntries(ready, &readyEntries, s, n);

  ConfChange cc1, cc2;
  string ccdata1, ccdata2;

  cc1.set_type(ConfChangeAddNode);
  cc1.set_nodeid(1);
  cc1.SerializeToString(&ccdata1);
  n->ProposeConfChange(cc1, &ready);
  applyReadyEntries(ready, &readyEntries, s, n);
  
  // try add the same node again
  n->ProposeConfChange(cc1, &ready);
  applyReadyEntries(ready, &readyEntries, s, n);

  // the new node join should be ok
  cc2.set_type(ConfChangeAddNode);
  cc2.set_nodeid(2);
  cc2.SerializeToString(&ccdata2);
  n->ProposeConfChange(cc2, &ready);
  applyReadyEntries(ready, &readyEntries, s, n);

  EXPECT_EQ((int)readyEntries.size(), 4);
  EXPECT_EQ(readyEntries[1].data(), ccdata1);
  EXPECT_EQ(readyEntries[3].data(), ccdata2);
}
