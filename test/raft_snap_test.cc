/*
 * Copyright (C) lichuang
 */

#include <gtest/gtest.h>
#include <math.h>
#include "libraft.h"
#include "raft_test_util.h"
#include "base/default_logger.h"
#include "base/util.h"
#include "core/progress.h"
#include "core/raft.h"
#include "core/read_only.h"
#include "storage/memory_storage.h"

using namespace libraft;

Snapshot testingSnap() {
  Snapshot ts;
  ts.mutable_metadata()->set_index(11);
  ts.mutable_metadata()->set_term(11);
  ts.mutable_metadata()->mutable_conf_state()->add_nodes(1);
  ts.mutable_metadata()->mutable_conf_state()->add_nodes(2);

  return ts;
}

TEST(raftPaperTests, TestSendingSnapshotSetPendingSnapshot) {
  vector<uint64_t> peers = {1};
  Storage *s = new MemoryStorage(&kDefaultLogger);
  raft *r = newTestRaft(1, peers, 10, 1, s);
  r->restore(testingSnap());
  r->becomeCandidate();
  r->becomeLeader();

 	// force set the next of node 1, so that
	// node 1 needs a snapshot
  r->progressMap_[2]->next_ = r->raftLog_->firstIndex();

  {
    Message msg = initMessage(2,1,MsgAppResp,NULL, r->progressMap_[2]->next_ - 1);
    msg.set_reject(true);

    r->step(msg);
  }

  EXPECT_EQ((int)r->progressMap_[2]->pendingSnapshot_, 11);

  delete r;
}

TEST(raftPaperTests, TestPendingSnapshotPauseReplication) {
  vector<uint64_t> peers = {1,2};
  Storage *s = new MemoryStorage(&kDefaultLogger);
  raft *r = newTestRaft(1, peers, 10, 1, s);
  r->restore(testingSnap());
  r->becomeCandidate();
  r->becomeLeader();

  r->progressMap_[2]->becomeSnapshot(11);

  {
    EntryVec entries = {initEntry(0,0,"somedata")};

    Message msg = initMessage(1,1,MsgProp,&entries);
    r->step(msg);
  }

  MessageVec msgs;
  r->readMessages(&msgs);
  EXPECT_EQ((int)msgs.size(), 0);

  delete r;
}

TEST(raftPaperTests, TestSnapshotFailure) {
  vector<uint64_t> peers = {1,2};
  Storage *s = new MemoryStorage(&kDefaultLogger);
  raft *r = newTestRaft(1, peers, 10, 1, s);
  r->restore(testingSnap());
  r->becomeCandidate();
  r->becomeLeader();

  r->progressMap_[2]->next_ = 1;
  r->progressMap_[2]->becomeSnapshot(11);

  {
    Message msg = initMessage(2,1,MsgSnapStatus,NULL);
    msg.set_reject(true);
    r->step(msg);
  }

  EXPECT_EQ((int)r->progressMap_[2]->pendingSnapshot_, 0);
  EXPECT_EQ((int)r->progressMap_[2]->next_, 1);
  EXPECT_TRUE(r->progressMap_[2]->paused_);

  delete r;
}

TEST(raftPaperTests, TestSnapshotSucceed) {
  vector<uint64_t> peers = {1,2};
  Storage *s = new MemoryStorage(&kDefaultLogger);
  raft *r = newTestRaft(1, peers, 10, 1, s);
  r->restore(testingSnap());
  r->becomeCandidate();
  r->becomeLeader();

  r->progressMap_[2]->next_ = 1;
  r->progressMap_[2]->becomeSnapshot(11);

  {
    Message msg = initMessage(2,1,MsgSnapStatus,NULL);
    msg.set_reject(false);
    r->step(msg);
  }

  EXPECT_EQ((int)r->progressMap_[2]->pendingSnapshot_, 0);
  EXPECT_EQ((int)r->progressMap_[2]->next_, 12);
  EXPECT_TRUE(r->progressMap_[2]->paused_);

  delete r;
}

TEST(raftPaperTests, TestSnapshotAbort) {
  vector<uint64_t> peers = {1,2};
  Storage *s = new MemoryStorage(&kDefaultLogger);
  raft *r = newTestRaft(1, peers, 10, 1, s);
  r->restore(testingSnap());
  r->becomeCandidate();
  r->becomeLeader();

  r->progressMap_[2]->next_ = 1;
  r->progressMap_[2]->becomeSnapshot(11);

	// A successful msgAppResp that has a higher/equal index than the
	// pending snapshot should abort the pending snapshot.
  {
    Message msg = initMessage(2,1,MsgAppResp,NULL,11);
    r->step(msg);
  }

  EXPECT_EQ((int)r->progressMap_[2]->pendingSnapshot_, 0);
  EXPECT_EQ((int)r->progressMap_[2]->next_, 12);

  delete r;
}
