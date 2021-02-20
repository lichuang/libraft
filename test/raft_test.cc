/*
 * Copyright (C) lichuang
 */

#include <gtest/gtest.h>
#include <math.h>
#include "libraft.h"
#include "raft_test_util.h"
#include "base/default_logger.h"
#include "base/util.h"
#include "core/raft.h"
#include "core/progress.h"
#include "core/read_only.h"
#include "storage/memory_storage.h"

using namespace libraft;

stateMachine *nopStepper = new blackHole();

void preVoteConfig(Config *c) {
  c->preVote = true; 
}

raftStateMachine* entsWithConfig(ConfigFun fun, const vector<uint64_t>& terms) {
  MemoryStorage *s = new MemoryStorage(&kDefaultLogger);

  vector<Entry> entries;
  size_t i;
  for (i = 0; i < terms.size(); ++i) {
    Entry entry;
    entry.set_index(i + 1);
    entry.set_term(terms[i]);
    entries.push_back(entry);
  }
  s->Append(entries);

  vector<uint64_t> peers;
  Config *c = newTestConfig(1, peers, 5, 1, s);
  if (fun != NULL) {
    fun(c);
  }
  raftStateMachine *sm = new raftStateMachine(c);
  raft *r = (raft*)sm->data();
  r->reset(terms[terms.size() - 1]);
  return sm;
}

// votedWithConfig creates a raft state machine with Vote and Term set
// to the given value but no log entries (indicating that it voted in
// the given term but has not received any logs).
raftStateMachine* votedWithConfig(ConfigFun fun, uint64_t vote, uint64_t term) {
  MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
  HardState hs;
  hs.set_vote(vote);
  hs.set_term(term);
  s->SetHardState(hs);
  Config *c = newTestConfig(1, vector<uint64_t>(), 5, 1, s);
  if (fun != NULL) {
    fun(c);
  }
  
  raftStateMachine *sm = new raftStateMachine(c);
  raft *r = (raft*)sm->data();
  r->reset(term);
  return sm;
}

TEST(raftTests, TestProgressBecomeProbe) {
  uint64_t match = 1;
  struct tmp {
    Progress p;
    uint64_t wnext;

    tmp(Progress p, uint64_t next)
      : p(p), wnext(next) {
    }
  };

  vector<tmp> tests;
  {
    Progress p(5, 256, &kDefaultLogger);
    p.state_ = ProgressStateReplicate;
    p.match_ = match;
    tests.push_back(tmp(p, 2));
  }
  // snapshot finish
  {
    Progress p(5, 256, &kDefaultLogger);
    p.state_ = ProgressStateSnapshot;
    p.match_ = match;
    p.pendingSnapshot_ = 10;
    tests.push_back(tmp(p, 11));
  }
  // snapshot failure
  {
    Progress p(5, 256, &kDefaultLogger);
    p.state_ = ProgressStateSnapshot;
    p.match_ = match;
    p.pendingSnapshot_ = 0;
    tests.push_back(tmp(p, 2));
  }

  size_t i;
  for (i = 0; i < tests.size(); ++i) {
    tmp &t = tests[i];
    t.p.becomeProbe();
    EXPECT_EQ(t.p.state_, ProgressStateProbe);
    EXPECT_EQ(t.p.match_, match);
    EXPECT_EQ(t.p.next_, t.wnext);
  }
}

TEST(raftTests, TestProgressBecomeReplicate) {
  Progress p(5, 256, &kDefaultLogger);
  p.state_ = ProgressStateProbe;
  p.match_ = 1;

  p.becomeReplicate();
  EXPECT_EQ(p.state_, ProgressStateReplicate);
  EXPECT_EQ((int)p.match_, 1);
  EXPECT_EQ(p.next_, p.match_ + 1);
}

TEST(raftTests, TestProgressBecomeSnapshot) {
  Progress p(5, 256, &kDefaultLogger);
  p.state_ = ProgressStateProbe;
  p.match_ = 1;

  p.becomeSnapshot(10);
  EXPECT_EQ(p.state_, ProgressStateSnapshot);
  EXPECT_EQ((int)p.match_, 1);
  EXPECT_EQ((int)p.pendingSnapshot_, 10);
}

TEST(raftTests, TestProgressUpdate) {
  uint64_t prevM = 3;
  uint64_t prevN = 5;

  struct tmp {
    uint64_t update;
    uint64_t wm;
    uint64_t wn;
    bool     wok;

    tmp(uint64_t update, uint64_t wm, uint64_t wn, bool ok)
      : update(update), wm(wm), wn(wn), wok(ok) {
    }
  };

  vector<tmp> tests;
  // do not decrease match, next
  tests.push_back(tmp(prevM - 1, prevM, prevN, false));
  // do not decrease next
  tests.push_back(tmp(prevM, prevM, prevN, false));
  // increase match, do not decrease next
  tests.push_back(tmp(prevM + 1, prevM + 1, prevN, true));
  // increase match, next
  tests.push_back(tmp(prevM + 2, prevM + 2, prevN + 1, true));

  size_t i;
  for (i = 0; i < tests.size(); ++i) {
    tmp &t = tests[i];
    Progress p(prevN, 256, &kDefaultLogger);
    p.match_ = prevM;

    bool ok = p.maybeUpdate(t.update);
    EXPECT_EQ(ok, t.wok);
    EXPECT_EQ(p.match_, t.wm);
    EXPECT_EQ(p.next_, t.wn);
  }
}

TEST(raftTests, TestProgressMaybeDecr) {
  struct tmp {
    ProgressState state;
    uint64_t m;
    uint64_t n;
    uint64_t rejected;
    uint64_t last;
    bool w;
    uint64_t wn;

    tmp(ProgressState s, uint64_t m, uint64_t n, uint64_t rejected, uint64_t last, bool w, uint64_t wn)
      : state(s), m(m), n(n), rejected(rejected), last(last), w(w), wn(wn) {}
  };

  vector<tmp> tests;
  // state replicate and rejected is not greater than match
  tests.push_back(tmp(ProgressStateReplicate, 5, 10, 5, 5, false, 10));
  // state replicate and rejected is not greater than match
  tests.push_back(tmp(ProgressStateReplicate, 5, 10, 4, 4, false, 10));
  // state replicate and rejected is greater than match
  // directly decrease to match+1
  tests.push_back(tmp(ProgressStateReplicate, 5, 10, 9, 9, true,  6));
  // next-1 != rejected is always false
  tests.push_back(tmp(ProgressStateProbe, 0, 0, 0, 0, false,  0));
  // next-1 != rejected is always false
  tests.push_back(tmp(ProgressStateProbe, 0, 10, 5, 5, false, 10));
  // next>1 = decremented by 1
  tests.push_back(tmp(ProgressStateProbe, 0, 10, 9, 9, true, 9));
  // next>1 = decremented by 1
  tests.push_back(tmp(ProgressStateProbe, 0, 2, 1, 1, true, 1));
  // next<=1 = reset to 1
  tests.push_back(tmp(ProgressStateProbe, 0, 1, 0, 0, true, 1));
  // decrease to min(rejected, last+1)
  tests.push_back(tmp(ProgressStateProbe, 0, 10, 9, 2, true, 3));
  // rejected < 1, reset to 1
  tests.push_back(tmp(ProgressStateProbe, 0, 10, 9, 0, true, 1));
  size_t i;
  for (i = 0; i < tests.size(); ++i) {
    tmp &t = tests[i];
    Progress p(t.n, 256, &kDefaultLogger);
    p.match_ = t.m;
    p.state_ = t.state;

    bool g = p.maybeDecrTo(t.rejected, t.last);
    EXPECT_EQ(g, t.w) << "i: " << i;
    EXPECT_EQ(p.match_, t.m);
    EXPECT_EQ(p.next_, t.wn);
  }
}

TEST(raftTests, TestProgressIsPaused) {
  struct tmp {
    ProgressState state;
    bool paused;
    bool w;

    tmp(ProgressState s, bool paused, bool w)
      : state(s), paused(paused), w(w) {}
  };

  vector<tmp> tests;
  tests.push_back(tmp(ProgressStateProbe, false, false));
  tests.push_back(tmp(ProgressStateProbe, true, true));
  tests.push_back(tmp(ProgressStateReplicate, false, false));
  tests.push_back(tmp(ProgressStateReplicate, true, false));
  tests.push_back(tmp(ProgressStateSnapshot, false, true));
  tests.push_back(tmp(ProgressStateSnapshot, true, true));
  size_t i;
  for (i = 0; i < tests.size(); ++i) {
    tmp &t = tests[i];
    Progress p(0, 256, &kDefaultLogger);
    p.paused_ = t.paused;
    p.state_ = t.state;

    bool g = p.isPaused();
    EXPECT_EQ(g, t.w) << "i: " << i;
  }
}

// TestProgressResume ensures that progress.maybeUpdate and progress.maybeDecrTo
// will reset progress.paused.
TEST(raftTests, TestProgressResume) {
  Progress p(2, 256, &kDefaultLogger);
  p.paused_ = true;

  p.maybeDecrTo(1, 1);
  EXPECT_FALSE(p.paused_);

  p.paused_ = true;
  p.maybeUpdate(2);
  EXPECT_FALSE(p.paused_);
}

// TestProgressResumeByHeartbeatResp ensures raft.heartbeat reset progress.paused by heartbeat response.
TEST(raftTests, TestProgressResumeByHeartbeatResp) {
  vector<uint64_t> peers;
  peers.push_back(1);
  peers.push_back(2);
  Storage *s = new MemoryStorage(&kDefaultLogger);
  raft *r = newTestRaft(1, peers, 5, 1, s);
  r->becomeCandidate();
  r->becomeLeader();
  r->progressMap_[2]->paused_ = true;

  {
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgBeat);

    r->step(msg);
    EXPECT_TRUE(r->progressMap_[2]->paused_);
  }

  r->progressMap_[2]->becomeReplicate();
  {
    Message msg;
    msg.set_from(2);
    msg.set_to(1);
    msg.set_type(MsgHeartbeatResp);

    r->step(msg);
    EXPECT_FALSE(r->progressMap_[2]->paused_);
  }

  delete s;
  delete r;
}

TEST(raftTests, TestProgressPaused) {
  vector<uint64_t> peers;
  peers.push_back(1);
  peers.push_back(2);
  Storage *s = new MemoryStorage(&kDefaultLogger);
  raft *r = newTestRaft(1, peers, 5, 1, s);

  r->becomeCandidate();
  r->becomeLeader();

  {
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgProp);
    Entry *entry = msg.add_entries();
    entry->set_data("somedata");
    r->step(msg);
  }
  {
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgProp);
    Entry *entry = msg.add_entries();
    entry->set_data("somedata");
    r->step(msg);
  }
  {
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgProp);
    Entry *entry = msg.add_entries();
    entry->set_data("somedata");
    r->step(msg);
  }

  vector<Message *> msgs;
  r->readMessages(&msgs);
  EXPECT_EQ((int)msgs.size(), 1);

  delete s;
  delete r;
}

void testLeaderElection(bool prevote) {
  ConfigFun fun = NULL;
  if (prevote) {
    fun = &preVoteConfig;
  }
  struct tmp {
    network *net;
    StateType state;
    uint64_t expTerm;

    tmp(network *net, StateType state, uint64_t t)
      : net(net), state(state), expTerm(t) {}
  };

  vector<tmp> tests;
  {
    vector<stateMachine*> peers;
    peers.push_back(NULL);
    peers.push_back(NULL);
    peers.push_back(NULL);
    tmp t(newNetworkWithConfig(fun, peers), StateLeader, 1);
    tests.push_back(t);
  }
  {
    vector<stateMachine*> peers;
    peers.push_back(NULL);
    peers.push_back(NULL);
    peers.push_back(nopStepper);
    tmp t(newNetworkWithConfig(fun, peers), StateLeader, 1);
    tests.push_back(t);
  }
  {
    vector<stateMachine*> peers;
    peers.push_back(NULL);
    peers.push_back(nopStepper);
    peers.push_back(nopStepper);
    tmp t(newNetworkWithConfig(fun, peers), StateCandidate, 1);
    tests.push_back(t);
  }
  {
    vector<stateMachine*> peers;
    peers.push_back(NULL);
    peers.push_back(nopStepper);
    peers.push_back(nopStepper);
    peers.push_back(NULL);
    tmp t(newNetworkWithConfig(fun, peers), StateCandidate, 1);
    tests.push_back(t);
  }
  {
    vector<stateMachine*> peers;
    peers.push_back(NULL);
    peers.push_back(nopStepper);
    peers.push_back(nopStepper);
    peers.push_back(NULL);
    peers.push_back(NULL);
    tmp t(newNetworkWithConfig(fun, peers), StateLeader, 1);
    tests.push_back(t);
  }
  // three logs further along than 0, but in the same term so rejections
  // are returned instead of the votes being ignored.
  {
    vector<uint64_t> terms;
    terms.push_back(1);

    vector<stateMachine*> peers;
    peers.push_back(NULL);
    peers.push_back(entsWithConfig(fun, terms));
    peers.push_back(entsWithConfig(fun, terms));
    terms.push_back(1);
    peers.push_back(entsWithConfig(fun, terms));
    peers.push_back(NULL);
    tmp t(newNetworkWithConfig(fun, peers), StateFollower, 1);
    tests.push_back(t);
  }
  size_t i;
  for (i = 0; i < tests.size(); ++i) {
    tmp& t = tests[i];
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgHup);
    vector<Message> msgs;
    msgs.push_back(msg);
    t.net->send(&msgs);
    raft *r = (raft*)t.net->peers[1]->data();
    StateType expState;
    uint64_t expTerm;
    if (t.state == StateCandidate && prevote) {
      // In pre-vote mode, an election that fails to complete
      // leaves the node in pre-candidate state without advancing
      // the term. 
      expState = StatePreCandidate;
      expTerm = 0;
    } else {
      expState = t.state;
      expTerm  = t.expTerm;
    }

    EXPECT_EQ(r->state_, expState) << "i: " << i;
    EXPECT_EQ(r->term_, expTerm);
  }
}

TEST(raftTests, TestLeaderElection) {
  testLeaderElection(false);
}

TEST(raftTests, TestLeaderElectionPreVote) {
  testLeaderElection(true);
}

// testLeaderCycle verifies that each node in a cluster can campaign
// and be elected in turn. This ensures that elections (including
// pre-vote) work when not starting from a clean slate (as they do in
// TestLeaderElection)
void testLeaderCycle(bool prevote) {
  ConfigFun fun = NULL;
  if (prevote) {
    fun = &preVoteConfig;
  }
  vector<stateMachine*> peers;
  peers.push_back(NULL);
  peers.push_back(NULL);
  peers.push_back(NULL);

  network *net = newNetworkWithConfig(fun, peers);
  size_t i;
  for (i = 1; i <= 3; i++) {
    Message msg;
    msg.set_from(i);
    msg.set_to(i);
    msg.set_type(MsgHup);
    vector<Message> msgs;
    msgs.push_back(msg);
    net->send(&msgs);

    map<uint64_t, stateMachine*>::iterator iter;
    for (iter = net->peers.begin(); iter != net->peers.end(); ++iter) {
      stateMachine *s = iter->second;
      raft *r = (raft*)s->data();
      EXPECT_FALSE(r->id_ == i && r->state_ != StateLeader);
      EXPECT_FALSE(r->id_ != i && r->state_ != StateFollower);
    }
  }
}

TEST(raftTests, TestLeaderCycle) {
  testLeaderCycle(false);
}

TEST(raftTests, TestLeaderCyclePreVote) {
  testLeaderCycle(true);
}

void testLeaderElectionOverwriteNewerLogs(bool preVote) {
  ConfigFun fun = NULL;
  if (preVote) {
    fun = &preVoteConfig;
  }
  // This network represents the results of the following sequence of
  // events:
  // - Node 1 won the election in term 1.
  // - Node 1 replicated a log entry to node 2 but died before sending
  //   it to other nodes.
  // - Node 3 won the second election in term 2.
  // - Node 3 wrote an entry to its logs but died without sending it
  //   to any other nodes.
  //
  // At this point, nodes 1, 2, and 3 all have uncommitted entries in
  // their logs and could win an election at term 3. The winner's log
  // entry overwrites the losers'. (TestLeaderSyncFollowerLog tests
  // the case where older log entries are overwritten, so this test
  // focuses on the case where the newer entries are lost).
  vector<stateMachine*> peers;
  {
    // Node 1: Won first election
    vector<uint64_t> terms;
    terms.push_back(1);
    peers.push_back(entsWithConfig(fun, terms));
  }
  {
    // Node 2: Got logs from node 1
    vector<uint64_t> terms;
    terms.push_back(1);
    peers.push_back(entsWithConfig(fun, terms));
  }
  {
    // Node 3: Won second election
    vector<uint64_t> terms;
    terms.push_back(2);
    peers.push_back(entsWithConfig(fun, terms));
  }
  {
    // Node 4: Voted but didn't get logs
    peers.push_back(votedWithConfig(fun, 3, 2));
  }
  {
    // Node 5: Voted but didn't get logs
    peers.push_back(votedWithConfig(fun, 3, 2));
  }
  network *net = newNetworkWithConfig(fun, peers);

  // Node 1 campaigns. The election fails because a quorum of nodes
  // know about the election that already happened at term 2. Node 1's
  // term is pushed ahead to 2.
  {
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgHup);
    vector<Message> msgs;
    msgs.push_back(msg);
    net->send(&msgs);
  }
  
  raft *r1 = (raft*)net->peers[1]->data();
  EXPECT_EQ(r1->state_, StateFollower);
  EXPECT_EQ((int)r1->term_, 2);

  // Node 1 campaigns again with a higher term. This time it succeeds.
  {
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgHup);
    vector<Message> msgs;
    msgs.push_back(msg);
    net->send(&msgs);
  }
  EXPECT_EQ(r1->state_, StateLeader);
  EXPECT_EQ((int)r1->term_, 3);

  // Now all nodes agree on a log entry with term 1 at index 1 (and
  // term 3 at index 2).
  map<uint64_t, stateMachine*>::iterator iter;
  for (iter = net->peers.begin(); iter != net->peers.end(); ++iter) {
    raft *r = (raft*)iter->second->data();
    EntryVec entries;
    r->raftLog_->allEntries(&entries);
    EXPECT_EQ((int)entries.size(), 2);
    EXPECT_EQ((int)entries[0].term(), 1);
    EXPECT_EQ((int)entries[1].term(), 3);
  }
}

// TestLeaderElectionOverwriteNewerLogs tests a scenario in which a
// newly-elected leader does *not* have the newest (i.e. highest term)
// log entries, and must overwrite higher-term log entries with
// lower-term ones.
TEST(raftTests, TestLeaderElectionOverwriteNewerLogs) {
  testLeaderElectionOverwriteNewerLogs(false);
}

TEST(raftTests, TestLeaderElectionOverwriteNewerLogsPreVote) {
  testLeaderElectionOverwriteNewerLogs(true);
}

void testVoteFromAnyState(MessageType vt) {
  vector<uint64_t> peers;
  peers.push_back(1);
  peers.push_back(2);
  peers.push_back(3);

  size_t i;
  for (i = 0; i < (int)NumStateType; ++i) {
    raft *r = newTestRaft(1, peers, 10, 1, new MemoryStorage(&kDefaultLogger));
    r->term_ = 1;

    StateType st = (StateType)i;
    switch (st) {
    case StateFollower:
      r->becomeFollower(r->term_, 3);
      break;
    case StatePreCandidate:
      r->becomePreCandidate();
      break;
    case StateCandidate:
      r->becomeCandidate();
      break;
    case StateLeader:
      r->becomeCandidate();
      r->becomeLeader();
      break;
    default:
      break;
    }

    // Note that setting our state above may have advanced r.Term
    // past its initial value.
    uint64_t origTerm = r->term_;
    uint64_t newTerm  = r->term_ + 1;

    Message msg;
    msg.set_from(2);
    msg.set_to(1);
    msg.set_type(vt);
    msg.set_term(newTerm);
    msg.set_logterm(newTerm);
    msg.set_index(42);
    int err = r->step(msg);

    EXPECT_EQ(err, OK);
    EXPECT_EQ((int)r->outMsgs_.size(), 1);
    Message *resp = r->outMsgs_[0];
    EXPECT_EQ(resp->type(), voteRespMsgType(vt));
    EXPECT_FALSE(resp->reject());

    if (vt == MsgVote) {
      // If this was a real vote, we reset our state and term.
      EXPECT_EQ(r->state_, StateFollower);
      EXPECT_EQ(r->term_, newTerm);
      EXPECT_EQ((int)r->vote_, 2);
    } else {
      // In a prevote, nothing changes.
      EXPECT_EQ(r->state_, st);
      EXPECT_EQ(r->term_, origTerm);
      // if st == StateFollower or StatePreCandidate, r hasn't voted yet.
      // In StateCandidate or StateLeader, it's voted for itself.
      EXPECT_FALSE(r->vote_ != kEmptyPeerId && r->vote_ != 1);
    }
  }
}

TEST(raftTests, TestVoteFromAnyState) {
  testVoteFromAnyState(MsgVote);
}

TEST(raftTests, TestPreVoteFromAnyState) {
  testVoteFromAnyState(MsgPreVote);
}

TEST(raftTests, TestLogReplication) {
  struct tmp {
    network *net;
    vector<Message> msgs;
    uint64_t wcommitted;

    tmp(network *net, vector<Message> msgs, uint64_t w)
      : net(net), msgs(msgs), wcommitted(w) {
    }
  };

  vector<tmp> tests;
  {
    vector<stateMachine*> peers;
    peers.push_back(NULL);
    peers.push_back(NULL);
    peers.push_back(NULL);

    vector<Message> msgs;
    {
      Message msg;
      msg.set_from(1);
      msg.set_to(1);
      msg.set_type(MsgProp);
      Entry *entry = msg.add_entries();
      entry->set_data("somedata");

      msgs.push_back(msg);
    }
    tests.push_back(tmp(newNetwork(peers), msgs, 2));
  }
  {
    vector<stateMachine*> peers;
    peers.push_back(NULL);
    peers.push_back(NULL);
    peers.push_back(NULL);

    vector<Message> msgs;
    {
      Message msg;
      msg.set_from(1);
      msg.set_to(1);
      msg.set_type(MsgProp);
      Entry *entry = msg.add_entries();
      entry->set_data("somedata");

      msgs.push_back(msg);
    }
    {
      Message msg;
      msg.set_from(1);
      msg.set_to(2);
      msg.set_type(MsgHup);

      msgs.push_back(msg);
    }
    {
      Message msg;
      msg.set_from(1);
      msg.set_to(2);
      msg.set_type(MsgProp);

      Entry *entry = msg.add_entries();
      entry->set_data("somedata");
      msgs.push_back(msg);
    }
    tests.push_back(tmp(newNetwork(peers), msgs, 4));
  }
  size_t i;
  for (i = 0; i < tests.size(); ++i) {
    tmp &t = tests[i];
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgHup);
    vector<Message> msgs;
    msgs.push_back(msg);
    t.net->send(&msgs);

    size_t j;
    for (j = 0; j < t.msgs.size(); ++j) {
      vector<Message> tmp_msgs;
      tmp_msgs.push_back(t.msgs[j]);
      t.net->send(&tmp_msgs);
    }

    map<uint64_t, stateMachine*>::iterator iter;
    for (iter = t.net->peers.begin(); iter != t.net->peers.end(); ++iter) {
      raft *r = (raft*)iter->second->data();
      EXPECT_EQ(r->raftLog_->committed_, t.wcommitted);
      
      EntryVec entries, ents;
      nextEnts(r, t.net->storage[iter->first], &entries);
      size_t m;
      for (m = 0; m < entries.size(); ++m) {
        if (entries[m].has_data()) {
          ents.push_back(entries[m]);
        }
      }

      vector<Message> props;
      for (m = 0; m < t.msgs.size(); ++m) {
        const Message& tmp_msgs = t.msgs[m];
        if (tmp_msgs.type() == MsgProp) {
          props.push_back(tmp_msgs);
        }
      } 
      for (m = 0; m < props.size(); ++m) {
        const Message& tmp_msgs = props[m];
        EXPECT_EQ(ents[m].data(),  tmp_msgs.entries(0).data());
      }
    }
  }
}

TEST(raftTests, TestSingleNodeCommit) {
  vector<stateMachine*> peers;
  peers.push_back(NULL);

  network *net = newNetwork(peers);
  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgHup);
    msgs.push_back(msg);
    net->send(&msgs);
  }
  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgProp);
    Entry *entry = msg.add_entries();
    entry->set_data("some data");
    msgs.push_back(msg);
    net->send(&msgs);
  }
  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgProp);
    Entry *entry = msg.add_entries();
    entry->set_data("some data");
    msgs.push_back(msg);
    net->send(&msgs);
  }

  raft *r = (raft*)net->peers[1]->data();
  EXPECT_EQ((int)r->raftLog_->committed_, 3);
}

// TestCannotCommitWithoutNewTermEntry tests the entries cannot be committed
// when leader changes, no new proposal comes in and ChangeTerm proposal is
// filtered.
TEST(raftTests, TestCannotCommitWithoutNewTermEntry) {
  vector<stateMachine*> peers;
  peers.push_back(NULL);
  peers.push_back(NULL);
  peers.push_back(NULL);
  peers.push_back(NULL);
  peers.push_back(NULL);

  network *net = newNetwork(peers);
  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgHup);
    msgs.push_back(msg);
    net->send(&msgs);
  }

  // 0 cannot reach 2,3,4
  net->cut(1,3); 
  net->cut(1,4); 
  net->cut(1,5); 

  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgProp);
    Entry *entry = msg.add_entries();
    entry->set_data("some data");
    msgs.push_back(msg);
    net->send(&msgs);
  }
  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgProp);
    Entry *entry = msg.add_entries();
    entry->set_data("some data");
    msgs.push_back(msg);
    net->send(&msgs);
  }

  raft *r = (raft*)net->peers[1]->data();
  EXPECT_EQ((int)r->raftLog_->committed_, 1);

  // network recovery
  net->recover();
  // avoid committing ChangeTerm proposal
  net->ignore(MsgApp);

  // elect 2 as the new leader with term 2
  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(2);
    msg.set_to(2);
    msg.set_type(MsgHup);
    msgs.push_back(msg);
    net->send(&msgs);
  }

  // no log entries from previous term should be committed
  r = (raft*)net->peers[2]->data();
  EXPECT_EQ((int)r->raftLog_->committed_, 1);

  net->recover();
  // send heartbeat; reset wait
  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(2);
    msg.set_to(2);
    msg.set_type(MsgBeat);
    msgs.push_back(msg);
    net->send(&msgs);
  }
  // append an entry at current term
  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(2);
    msg.set_to(2);
    msg.set_type(MsgProp);
    Entry *entry = msg.add_entries();
    entry->set_data("somedata");
    msgs.push_back(msg);
    net->send(&msgs);
  }
  EXPECT_EQ((int)r->raftLog_->committed_, 5);
}

// TestCommitWithoutNewTermEntry tests the entries could be committed
// when leader changes, no new proposal comes in.
TEST(raftTests, TestCommitWithoutNewTermEntry) {
  vector<stateMachine*> peers;
  peers.push_back(NULL);
  peers.push_back(NULL);
  peers.push_back(NULL);
  peers.push_back(NULL);
  peers.push_back(NULL);

  network *net = newNetwork(peers);
  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgHup);
    msgs.push_back(msg);
    net->send(&msgs);
  }

  // 0 cannot reach 2,3,4
  net->cut(1,3); 
  net->cut(1,4); 
  net->cut(1,5); 

  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgProp);
    Entry *entry = msg.add_entries();
    entry->set_data("some data");
    msgs.push_back(msg);
    net->send(&msgs);
  }
  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgProp);
    Entry *entry = msg.add_entries();
    entry->set_data("some data");
    msgs.push_back(msg);
    net->send(&msgs);
  }

  raft *r = (raft*)net->peers[1]->data();
  EXPECT_EQ((int)r->raftLog_->committed_, 1);

  // network recovery
  net->recover();

  // elect 1 as the new leader with term 2
  // after append a ChangeTerm entry from the current term, all entries
  // should be committed
  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(2);
    msg.set_to(2);
    msg.set_type(MsgHup);
    msgs.push_back(msg);
    net->send(&msgs);
  }

  EXPECT_EQ((int)r->raftLog_->committed_, 4);
}

TEST(raftTests, TestDuelingCandidates) {
  vector<stateMachine*> peers;

  {
    vector<uint64_t> ids;
    ids.push_back(1);
    ids.push_back(2);
    ids.push_back(3);
    raft *r = newTestRaft(1, ids, 10, 1, new MemoryStorage(&kDefaultLogger));
    peers.push_back(new raftStateMachine(r)); 
  }
  {
    vector<uint64_t> ids;
    ids.push_back(1);
    ids.push_back(2);
    ids.push_back(3);
    raft *r = newTestRaft(2, ids, 10, 1, new MemoryStorage(&kDefaultLogger));
    peers.push_back(new raftStateMachine(r)); 
  }
  {
    vector<uint64_t> ids;
    ids.push_back(1);
    ids.push_back(2);
    ids.push_back(3);
    raft *r = newTestRaft(3, ids, 10, 1, new MemoryStorage(&kDefaultLogger));
    peers.push_back(new raftStateMachine(r)); 
  }
  network *net = newNetwork(peers);

  net->cut(1,3);

  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgHup);
    msgs.push_back(msg);
    net->send(&msgs);
  }
  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(3);
    msg.set_to(3);
    msg.set_type(MsgHup);
    msgs.push_back(msg);
    net->send(&msgs);
  }

  // 1 becomes leader since it receives votes from 1 and 2
  raft *r = (raft*)net->peers[1]->data();
  EXPECT_EQ(r->state_, StateLeader);

  // 3 stays as candidate since it receives a vote from 3 and a rejection from 2
  r = (raft*)net->peers[3]->data();
  EXPECT_EQ(r->state_, StateCandidate);

  net->recover();

  // candidate 3 now increases its term and tries to vote again
  // we expect it to disrupt the leader 1 since it has a higher term
  // 3 will be follower again since both 1 and 2 rejects its vote request since 3 does not have a long enough log
  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(3);
    msg.set_to(3);
    msg.set_type(MsgHup);
    msgs.push_back(msg);
    net->send(&msgs);
  }

  MemoryStorage *s = new MemoryStorage(&kDefaultLogger); 
  {
    EntryVec entries;
    entries.push_back(Entry());
    Entry entry;
    entry.set_term(1);
    entry.set_index(1);
    entries.push_back(entry);

    s->Append(entries);
  }

  raftLog *log = new raftLog(s, &kDefaultLogger);
  log->committed_ = 1;
  log->unstable_.offset_ = 2;

  struct tmp {
    raft *r;
    StateType state;
    uint64_t term;
    raftLog* log;

    tmp(raft *r, StateType state, uint64_t term, raftLog *log)
      : r(r), state(state), term(term), log(log) {}
  };

  vector<tmp> tests;
  tests.push_back(tmp((raft*)peers[0]->data(), StateFollower, 2, log)); 
  tests.push_back(tmp((raft*)peers[1]->data(), StateFollower, 2, log)); 
  tests.push_back(tmp((raft*)peers[2]->data(), StateFollower, 2, new raftLog(new MemoryStorage(&kDefaultLogger), &kDefaultLogger))); 

  size_t i;
  for (i = 0; i < tests.size(); ++i) {
    tmp& t = tests[i];
    EXPECT_EQ(t.r->state_, t.state);
    EXPECT_EQ(t.r->term_, t.term);

    string base = raftLogString(t.log);
    if (net->peers[i + 1]->type() == raftType) {
      raft *tmp_r = (raft*)net->peers[i + 1]->data();
      string str = raftLogString(tmp_r->raftLog_);
      EXPECT_EQ(base, str) << "i: " << i;
    }
  }
}

TEST(raftTests, TestDuelingPreCandidates) {
  vector<stateMachine*> peers;

  {
    vector<uint64_t> ids;
    ids.push_back(1);
    ids.push_back(2);
    ids.push_back(3);
    raft *r = newTestRaft(1, ids, 10, 1, new MemoryStorage(&kDefaultLogger));
    r->preVote_ = true;
    peers.push_back(new raftStateMachine(r)); 
  }
  {
    vector<uint64_t> ids;
    ids.push_back(1);
    ids.push_back(2);
    ids.push_back(3);
    raft *r = newTestRaft(2, ids, 10, 1, new MemoryStorage(&kDefaultLogger));
    r->preVote_ = true;
    peers.push_back(new raftStateMachine(r)); 
  }
  {
    vector<uint64_t> ids;
    ids.push_back(1);
    ids.push_back(2);
    ids.push_back(3);
    raft *r = newTestRaft(3, ids, 10, 1, new MemoryStorage(&kDefaultLogger));
    r->preVote_ = true;
    peers.push_back(new raftStateMachine(r)); 
  }
  network *net = newNetwork(peers);

  net->cut(1,3);

  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgHup);
    msgs.push_back(msg);
    net->send(&msgs);
  }
  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(3);
    msg.set_to(3);
    msg.set_type(MsgHup);
    msgs.push_back(msg);
    net->send(&msgs);
  }

  // 1 becomes leader since it receives votes from 1 and 2
  raft *r = (raft*)net->peers[1]->data();
  EXPECT_EQ(r->state_, StateLeader);

  // 3 campaigns then reverts to follower when its PreVote is rejected
  r = (raft*)net->peers[3]->data();
  EXPECT_EQ(r->state_, StateFollower);

  net->recover();

  // candidate 3 now increases its term and tries to vote again
  // we expect it to disrupt the leader 1 since it has a higher term
  // 3 will be follower again since both 1 and 2 rejects its vote request since 3 does not have a long enough log
  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(3);
    msg.set_to(3);
    msg.set_type(MsgHup);
    msgs.push_back(msg);
    net->send(&msgs);
  }

  MemoryStorage *s = new MemoryStorage(&kDefaultLogger); 
  {
    EntryVec entries;
    entries.push_back(Entry());
    Entry entry;
    entry.set_term(1);
    entry.set_index(1);
    entries.push_back(entry);

    s->Append(entries);
  }

  raftLog *log = new raftLog(s, &kDefaultLogger);
  log->committed_ = 1;
  log->unstable_.offset_ = 2;

  struct tmp {
    raft *r;
    StateType state;
    uint64_t term;
    raftLog* log;

    tmp(raft *r, StateType state, uint64_t term, raftLog *log)
      : r(r), state(state), term(term), log(log) {}
  };

  vector<tmp> tests;
  tests.push_back(tmp((raft*)peers[0]->data(), StateLeader, 1, log)); 
  tests.push_back(tmp((raft*)peers[1]->data(), StateFollower, 1, log)); 
  tests.push_back(tmp((raft*)peers[2]->data(), StateFollower, 1, new raftLog(new MemoryStorage(&kDefaultLogger), &kDefaultLogger))); 

  size_t i;
  for (i = 0; i < tests.size(); ++i) {
    tmp& t = tests[i];
    EXPECT_EQ(t.r->state_, t.state);
    EXPECT_EQ(t.r->term_, t.term);

    string base = raftLogString(t.log);
    if (net->peers[i + 1]->type() == raftType) {
      raft *tmp_r = (raft*)net->peers[i + 1]->data();
      string str = raftLogString(tmp_r->raftLog_);
      EXPECT_EQ(base, str) << "i: " << i;
    }
  }
}

TEST(raftTests, TestCandidateConcede) {
  vector<stateMachine*> peers;

  peers.push_back(NULL);
  peers.push_back(NULL);
  peers.push_back(NULL);

  network *net = newNetwork(peers);
  net->isolate(1);

  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgHup);
    msgs.push_back(msg);
    net->send(&msgs);
  }
  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(3);
    msg.set_to(3);
    msg.set_type(MsgHup);
    msgs.push_back(msg);
    net->send(&msgs);
  }

  // heal the partition
  net->recover();

  // send heartbeat; reset wait
  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(3);
    msg.set_to(3);
    msg.set_type(MsgBeat);
    msgs.push_back(msg);
    net->send(&msgs);
  }

  string data = "force follower";
  // send a proposal to 3 to flush out a MsgApp to 1
  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(3);
    msg.set_to(3);
    msg.set_type(MsgProp);
    Entry *entry = msg.add_entries();
    entry->set_data(data);
    msgs.push_back(msg);
    net->send(&msgs);
  }
  // send heartbeat; flush out commit
  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(3);
    msg.set_to(3);
    msg.set_type(MsgBeat);
    msgs.push_back(msg);
    net->send(&msgs);
  }

  raft *r = (raft*)net->peers[1]->data();
  EXPECT_EQ(r->state_, StateFollower);
  EXPECT_EQ((int)r->term_, 1);

  MemoryStorage *s = new MemoryStorage(&kDefaultLogger); 
  EntryVec entries;

  entries.push_back(Entry());
  {
    Entry entry;
    entry.set_term(1);
    entry.set_index(1);
    entries.push_back(entry);
  }
  {
    Entry entry;
    entry.set_term(1);
    entry.set_index(2);
    entry.set_data(data);
    entries.push_back(entry);
  }
  s->entries_.clear();
  s->entries_.insert(s->entries_.end(), entries.begin(), entries.end());

  raftLog *log = new raftLog(s, &kDefaultLogger);
  log->committed_ = 2;
  log->unstable_.offset_ = 3;
  string logStr = raftLogString(log);

  map<uint64_t, stateMachine*>::iterator iter;
  for (iter = net->peers.begin(); iter != net->peers.end(); ++iter) {
    stateMachine *tmp_s = iter->second;
    if (tmp_s->type() != raftType) {
      continue;
    }
    raft *tmp_r = (raft*)tmp_s->data();
    string str = raftLogString(tmp_r->raftLog_);
    EXPECT_EQ(str, logStr);
  }
}

TEST(raftTests, TestSingleNodeCandidate) {
  vector<stateMachine*> peers;

  peers.push_back(NULL);

  network *net = newNetwork(peers);

  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgHup);
    msgs.push_back(msg);
    net->send(&msgs);
  }
  raft *r = (raft*)net->peers[1]->data();
  EXPECT_EQ(r->state_, StateLeader);
}

TEST(raftTests, TestSingleNodePreCandidate) {
  vector<stateMachine*> peers;

  peers.push_back(NULL);

  network *net = newNetworkWithConfig(preVoteConfig, peers);

  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgHup);
    msgs.push_back(msg);
    net->send(&msgs);
  }
  raft *r = (raft*)net->peers[1]->data();
  EXPECT_EQ(r->state_, StateLeader);
}

TEST(raftTests, TestOldMessages) {
  vector<stateMachine*> peers;

  peers.push_back(NULL);
  peers.push_back(NULL);
  peers.push_back(NULL);

  network *net = newNetwork(peers);

  // make 0 leader @ term 3
  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgHup);
    msgs.push_back(msg);
    net->send(&msgs);
  }
  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(2);
    msg.set_to(2);
    msg.set_type(MsgHup);
    msgs.push_back(msg);
    net->send(&msgs);
  }
  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgHup);
    msgs.push_back(msg);
    net->send(&msgs);
  }
  // pretend we're an old leader trying to make progress; this entry is expected to be ignored.
  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(2);
    msg.set_to(1);
    msg.set_term(2);
    msg.set_type(MsgApp);
    Entry *entry = msg.add_entries();
    entry->set_index(3);
    entry->set_term(2);
    msgs.push_back(msg);
    net->send(&msgs);
  }
  // commit a new entry
  string data = "somedata";
  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgProp);
    Entry *entry = msg.add_entries();
    entry->set_data(data);
    msgs.push_back(msg);
    net->send(&msgs);
  }

  MemoryStorage *s = new MemoryStorage(&kDefaultLogger); 
  EntryVec entries;

  entries.push_back(Entry());
  {
    Entry entry;
    entry.set_term(1);
    entry.set_index(1);
    entries.push_back(entry);
  }
  {
    Entry entry;
    entry.set_term(2);
    entry.set_index(2);
    entries.push_back(entry);
  }
  {
    Entry entry;
    entry.set_term(3);
    entry.set_index(3);
    entries.push_back(entry);
  }
  {
    Entry entry;
    entry.set_term(3);
    entry.set_index(4);
    entry.set_data(data);
    entries.push_back(entry);
  }
  s->entries_.clear();
  s->entries_.insert(s->entries_.end(), entries.begin(), entries.end());
  raftLog *log = new raftLog(s, &kDefaultLogger);
  log->committed_ = 4;
  log->unstable_.offset_ = 5;
  string logStr = raftLogString(log);

  map<uint64_t, stateMachine*>::iterator iter;
  for (iter = net->peers.begin(); iter != net->peers.end(); ++iter) {
    stateMachine *tmp_s = iter->second;
    if (tmp_s->type() != raftType) {
      continue;
    }
    raft *tmp_r = (raft*)tmp_s->data();
    string str = raftLogString(tmp_r->raftLog_);
    EXPECT_EQ(str, logStr);
  }
}

TEST(raftTests, TestProposal) {
  struct tmp {
    network *net;
    bool success;

    tmp(network *net, bool success)
      : net(net), success(success) {
    }
  };

  vector<tmp> tests;
  {
    vector<stateMachine*> peers;
    peers.push_back(NULL);
    peers.push_back(NULL);
    peers.push_back(NULL);

    network *net = newNetwork(peers);
    tests.push_back(tmp(net, true));
  }
  {
    vector<stateMachine*> peers;
    peers.push_back(NULL);
    peers.push_back(NULL);
    peers.push_back(nopStepper);

    network *net = newNetwork(peers);
    tests.push_back(tmp(net, true));
  }
  {
    vector<stateMachine*> peers;
    peers.push_back(NULL);
    peers.push_back(nopStepper);
    peers.push_back(nopStepper);

    //network *net = newNetwork(peers);
    //tests.push_back(tmp(net, false));
  }
  {
    vector<stateMachine*> peers;
    peers.push_back(NULL);
    peers.push_back(nopStepper);
    peers.push_back(nopStepper);
    peers.push_back(NULL);

    //network *net = newNetwork(peers);
    //tests.push_back(tmp(net, false));
  }
  {
    vector<stateMachine*> peers;
    peers.push_back(NULL);
    peers.push_back(nopStepper);
    peers.push_back(nopStepper);
    peers.push_back(NULL);
    peers.push_back(NULL);

    network *net = newNetwork(peers);
    tests.push_back(tmp(net, true));
  }

  size_t i;
  for (i = 0; i < tests.size(); ++i) {
    tmp& t = tests[i];
    string data = "somedata";

    // promote 0 the leader
    {
      vector<Message> msgs;
      Message msg;
      msg.set_from(1);
      msg.set_to(1);
      msg.set_type(MsgHup);
      msgs.push_back(msg);
      t.net->send(&msgs);
    }
    {
      vector<Message> msgs;
      Message msg;
      msg.set_from(1);
      msg.set_to(1);
      msg.set_type(MsgProp);
      Entry *entry = msg.add_entries();
      entry->set_data(data);
      msgs.push_back(msg);
      t.net->send(&msgs);
    }

    string logStr = "";
    if (t.success) {
      MemoryStorage *s = new MemoryStorage(&kDefaultLogger); 
      EntryVec entries;

      entries.push_back(Entry());
      {
        Entry entry;
        entry.set_term(1);
        entry.set_index(1);
        entries.push_back(entry);
      }
      {
        Entry entry;
        entry.set_term(1);
        entry.set_index(2);
        entry.set_data(data);
        entries.push_back(entry);
      }
      s->entries_.clear();
      s->entries_.insert(s->entries_.end(), entries.begin(), entries.end());
      raftLog *log = new raftLog(s, &kDefaultLogger);
      log->committed_ = 2;
      log->unstable_.offset_ = 3;
      logStr = raftLogString(log);
    }

    map<uint64_t, stateMachine*>::iterator iter;
    for (iter = t.net->peers.begin(); iter != t.net->peers.end(); ++iter) {
      stateMachine *s = iter->second;
      if (s->type() != raftType) {
        continue;
      }
      raft *r = (raft*)s->data();
      string str = raftLogString(r->raftLog_);
      EXPECT_EQ(str, logStr);
    }
  }
}

TEST(raftTests, TestProposalByProxy) {
  vector<network*> tests;

  {
    vector<stateMachine*> peers;
    peers.push_back(NULL);
    peers.push_back(NULL);
    peers.push_back(NULL);

    network *net = newNetwork(peers);
    tests.push_back(net);
  }
  {
    vector<stateMachine*> peers;
    peers.push_back(NULL);
    peers.push_back(NULL);
    peers.push_back(nopStepper);

    network *net = newNetwork(peers);
    tests.push_back(net);
  }

  size_t i;
  string data = "somedata";
  for (i = 0; i < tests.size(); ++i) {
    network *net = tests[i];
    // promote 0 the leader
    {
      vector<Message> msgs;
      Message msg;
      msg.set_from(1);
      msg.set_to(1);
      msg.set_type(MsgHup);
      msgs.push_back(msg);
      net->send(&msgs);
    }
    // propose via follower
    {
      vector<Message> msgs;
      Message msg;
      msg.set_from(2);
      msg.set_to(2);
      msg.set_type(MsgProp);
      Entry *entry = msg.add_entries();
      entry->set_data(data);
      msgs.push_back(msg);
      net->send(&msgs);
    }

    string logStr = "";
    MemoryStorage *s = new MemoryStorage(&kDefaultLogger); 
    EntryVec entries;

    entries.push_back(Entry());
    {
      Entry entry;
      entry.set_term(1);
      entry.set_index(1);
      entries.push_back(entry);
    }
    {
      Entry entry;
      entry.set_term(1);
      entry.set_index(2);
      entry.set_data(data);
      entries.push_back(entry);
    }
    s->entries_.clear();
    s->entries_.insert(s->entries_.end(), entries.begin(), entries.end());
    raftLog *log = new raftLog(s, &kDefaultLogger);
    log->committed_ = 2;
    log->unstable_.offset_ = 3;
    logStr = raftLogString(log);

    map<uint64_t, stateMachine*>::iterator iter;
    for (iter = net->peers.begin(); iter != net->peers.end(); ++iter) {
      stateMachine *tmp_s = iter->second;
      if (tmp_s->type() != raftType) {
        continue;
      }
      raft *tmp_r = (raft*)tmp_s->data();
      string str = raftLogString(tmp_r->raftLog_);
      EXPECT_EQ(str, logStr);
    }
    EXPECT_EQ((int)((raft*)(net->peers[1]->data()))->term_, 1);
  }
}

TEST(raftTests, TestCommit) {
  struct tmp {
    vector<uint64_t> matches;
    vector<Entry> logs;
    uint64_t term;
    uint64_t w;

    tmp(vector<uint64_t> matches, vector<Entry> entries, uint64_t term, uint64_t w)
      : matches(matches), logs(entries), term(term), w(w) {
    }
  };

  vector<tmp> tests;
  // single
  {
    {
      vector<uint64_t> matches;
      vector<Entry> entries;

      matches.push_back(1);

      {
        Entry entry;
        entry.set_index(1);
        entry.set_term(1);
        entries.push_back(entry);
      }

      tests.push_back(tmp(matches, entries, 1, 1));
    }
    {
      vector<uint64_t> matches;
      vector<Entry> entries;

      matches.push_back(1);

      {
        Entry entry;
        entry.set_index(1);
        entry.set_term(1);
        entries.push_back(entry);
      }

      tests.push_back(tmp(matches, entries, 2, 0));
    }
    {
      vector<uint64_t> matches;
      vector<Entry> entries;

      matches.push_back(2);

      {
        Entry entry;
        entry.set_index(1);
        entry.set_term(1);
        entries.push_back(entry);
      }
      {
        Entry entry;
        entry.set_index(2);
        entry.set_term(2);
        entries.push_back(entry);
      }

      tests.push_back(tmp(matches, entries, 2, 2));
    }
    {
      vector<uint64_t> matches;
      vector<Entry> entries;

      matches.push_back(1);

      {
        Entry entry;
        entry.set_index(1);
        entry.set_term(2);
        entries.push_back(entry);
      }

      tests.push_back(tmp(matches, entries, 2, 1));
    }
  }
  // odd
  {
    {
      vector<uint64_t> matches;
      vector<Entry> entries;

      matches.push_back(2);
      matches.push_back(1);
      matches.push_back(1);

      {
        Entry entry;
        entry.set_index(1);
        entry.set_term(1);
        entries.push_back(entry);
      }
      {
        Entry entry;
        entry.set_index(2);
        entry.set_term(2);
        entries.push_back(entry);
      }

      tests.push_back(tmp(matches, entries, 1, 1));
    }
    {
      vector<uint64_t> matches;
      vector<Entry> entries;

      matches.push_back(2);
      matches.push_back(1);
      matches.push_back(1);

      {
        Entry entry;
        entry.set_index(1);
        entry.set_term(1);
        entries.push_back(entry);
      }
      {
        Entry entry;
        entry.set_index(2);
        entry.set_term(1);
        entries.push_back(entry);
      }

      tests.push_back(tmp(matches, entries, 2, 0));
    }
    {
      vector<uint64_t> matches;
      vector<Entry> entries;

      matches.push_back(2);
      matches.push_back(1);
      matches.push_back(2);

      {
        Entry entry;
        entry.set_index(1);
        entry.set_term(1);
        entries.push_back(entry);
      }
      {
        Entry entry;
        entry.set_index(2);
        entry.set_term(2);
        entries.push_back(entry);
      }

      tests.push_back(tmp(matches, entries, 2, 2));
    }
    {
      vector<uint64_t> matches;
      vector<Entry> entries;

      matches.push_back(2);
      matches.push_back(1);
      matches.push_back(2);

      {
        Entry entry;
        entry.set_index(1);
        entry.set_term(1);
        entries.push_back(entry);
      }
      {
        Entry entry;
        entry.set_index(2);
        entry.set_term(1);
        entries.push_back(entry);
      }

      tests.push_back(tmp(matches, entries, 2, 0));
    }
  }
  // even
  {
    {
      vector<uint64_t> matches;
      vector<Entry> entries;

      matches.push_back(2);
      matches.push_back(1);
      matches.push_back(1);
      matches.push_back(1);

      {
        Entry entry;
        entry.set_index(1);
        entry.set_term(1);
        entries.push_back(entry);
      }
      {
        Entry entry;
        entry.set_index(2);
        entry.set_term(2);
        entries.push_back(entry);
      }

      tests.push_back(tmp(matches, entries, 1, 1));
    }
    {
      vector<uint64_t> matches;
      vector<Entry> entries;

      matches.push_back(2);
      matches.push_back(1);
      matches.push_back(1);
      matches.push_back(1);

      {
        Entry entry;
        entry.set_index(1);
        entry.set_term(1);
        entries.push_back(entry);
      }
      {
        Entry entry;
        entry.set_index(2);
        entry.set_term(1);
        entries.push_back(entry);
      }

      tests.push_back(tmp(matches, entries, 2, 0));
    }
    {
      vector<uint64_t> matches;
      vector<Entry> entries;

      matches.push_back(2);
      matches.push_back(1);
      matches.push_back(1);
      matches.push_back(2);

      {
        Entry entry;
        entry.set_index(1);
        entry.set_term(1);
        entries.push_back(entry);
      }
      {
        Entry entry;
        entry.set_index(2);
        entry.set_term(2);
        entries.push_back(entry);
      }

      tests.push_back(tmp(matches, entries, 1, 1));
    }
    {
      vector<uint64_t> matches;
      vector<Entry> entries;

      matches.push_back(2);
      matches.push_back(1);
      matches.push_back(1);
      matches.push_back(2);

      {
        Entry entry;
        entry.set_index(1);
        entry.set_term(1);
        entries.push_back(entry);
      }
      {
        Entry entry;
        entry.set_index(2);
        entry.set_term(1);
        entries.push_back(entry);
      }

      tests.push_back(tmp(matches, entries, 2, 0));
    }
    {
      vector<uint64_t> matches;
      vector<Entry> entries;

      matches.push_back(2);
      matches.push_back(1);
      matches.push_back(2);
      matches.push_back(2);

      {
        Entry entry;
        entry.set_index(1);
        entry.set_term(1);
        entries.push_back(entry);
      }
      {
        Entry entry;
        entry.set_index(2);
        entry.set_term(2);
        entries.push_back(entry);
      }

      tests.push_back(tmp(matches, entries, 2, 2));
    }
    {
      vector<uint64_t> matches;
      vector<Entry> entries;

      matches.push_back(2);
      matches.push_back(1);
      matches.push_back(2);
      matches.push_back(2);

      {
        Entry entry;
        entry.set_index(1);
        entry.set_term(1);
        entries.push_back(entry);
      }
      {
        Entry entry;
        entry.set_index(2);
        entry.set_term(1);
        entries.push_back(entry);
      }

      tests.push_back(tmp(matches, entries, 2, 0));
    }
  }

  size_t i;
  for (i = 0; i < tests.size(); ++i) {
    tmp& t = tests[i];
    MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
    s->Append(t.logs);
    s->hardState_.set_term(t.term);

    vector<uint64_t> peers;
    peers.push_back(1);
    raft *r = newTestRaft(1, peers, 5, 1, s);
    size_t j;
    for (j = 0; j < t.matches.size(); ++j) {
      r->setProgress(j + 1, t.matches[j], t.matches[j] + 1);
    }
    r->maybeCommit();
    EXPECT_EQ(r->raftLog_->committed_, t.w);
  }
}

TEST(raftTests, TestPastElectionTimeout) {
  struct tmp {
    int elapse;
    float probability;
    bool round;

    tmp(int elapse, float probability, bool round)
      : elapse(elapse), probability(probability), round(round) {
    }
  };
  vector<tmp> tests;

  tests.push_back(tmp(5, 0, false));
  tests.push_back(tmp(10, 0.1, true));
  tests.push_back(tmp(13, 0.4, true));
  tests.push_back(tmp(15, 0.6, true));
  tests.push_back(tmp(18, 0.9, true));
  tests.push_back(tmp(20, 1, false));

  size_t i;
  for (i = 0; i < tests.size(); ++i) {
    tmp& t = tests[i];

    vector<uint64_t> peers;
    peers.push_back(1);
    MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
    raft *r = newTestRaft(1, peers, 10, 1, s);
    r->electionElapsed_ = t.elapse;
    int c = 0, j;
    for (j = 0; j < 10000; ++j) {
      r->resetRandomizedElectionTimeout();
      if (r->pastElectionTimeout()) {
        ++c;
      }
    }

    float g = (float)c / 10000.0;
    if (t.round) {
      g = floor(g * 10 + 0.5) / 10.0;
    }

    EXPECT_EQ(g, t.probability);
  }
}

// TestHandleMsgApp ensures:
// 1. Reply false if log doesn’t contain an entry at prevLogIndex whose term matches prevLogTerm.
// 2. If an existing entry conflicts with a new one (same index but different terms),
//    delete the existing entry and all that follow it; append any new entries not already in the log.
// 3. If leaderCommit > commitIndex, set commitIndex = min(leaderCommit, index of last new entry).
TEST(raftTests, TestHandleMsgApp) {
  struct tmp {
    Message m;
    uint64_t index;
    uint64_t commit;
    bool reject;

    tmp(Message m, uint64_t index, uint64_t commit, bool reject)
      : m(m), index(index), commit(commit), reject(reject) {}
  };

  vector<tmp> tests;
  // Ensure 1

  // previous log mismatch
  {
    Message msg;
    msg.set_type(MsgApp);
    msg.set_term(2);
    msg.set_logterm(3);
    msg.set_index(2);
    msg.set_commit(3);

    tests.push_back(tmp(msg, 2, 0, true));
  }
  // previous log non-exist
  {
    Message msg;
    msg.set_type(MsgApp);
    msg.set_term(2);
    msg.set_logterm(3);
    msg.set_index(3);
    msg.set_commit(3);

    tests.push_back(tmp(msg, 2, 0, true));
  }

  // Ensure 2
  {
    Message msg;
    msg.set_type(MsgApp);
    msg.set_term(2);
    msg.set_logterm(1);
    msg.set_index(1);
    msg.set_commit(1);

    tests.push_back(tmp(msg, 2, 1, false));
  }
  {
    Message msg;
    msg.set_type(MsgApp);
    msg.set_term(2);
    msg.set_logterm(0);
    msg.set_index(0);
    msg.set_commit(1);
    
    Entry *entry = msg.add_entries();
    entry->set_index(1);
    entry->set_term(2);

    tests.push_back(tmp(msg, 1, 1, false));
  }
  {
    Message msg;
    msg.set_type(MsgApp);
    msg.set_term(2);
    msg.set_logterm(2);
    msg.set_index(2);
    msg.set_commit(3);
    
    Entry *entry = msg.add_entries();
    entry->set_index(3);
    entry->set_term(2);

    entry = msg.add_entries();
    entry->set_index(4);
    entry->set_term(2);

    tests.push_back(tmp(msg, 4, 3, false));
  }
  {
    Message msg;
    msg.set_type(MsgApp);
    msg.set_term(2);
    msg.set_logterm(2);
    msg.set_index(2);
    msg.set_commit(4);
    
    Entry *entry = msg.add_entries();
    entry->set_index(3);
    entry->set_term(2);

    tests.push_back(tmp(msg, 3, 3, false));
  }
  {
    Message msg;
    msg.set_type(MsgApp);
    msg.set_term(2);
    msg.set_logterm(1);
    msg.set_index(1);
    msg.set_commit(4);
    
    Entry *entry = msg.add_entries();
    entry->set_index(2);
    entry->set_term(2);

    tests.push_back(tmp(msg, 2, 2, false));
  }

  // Ensure 3

  // match entry 1, commit up to last new entry 1
  {
    Message msg;
    msg.set_type(MsgApp);
    msg.set_term(1);
    msg.set_logterm(1);
    msg.set_index(1);
    msg.set_commit(3);

    tests.push_back(tmp(msg, 2, 1, false));
  }
  // match entry 1, commit up to last new entry 2
  {
    Message msg;
    msg.set_type(MsgApp);
    msg.set_term(1);
    msg.set_logterm(1);
    msg.set_index(1);
    msg.set_commit(3);

    Entry *entry = msg.add_entries();
    entry->set_index(2);
    entry->set_term(2);
    tests.push_back(tmp(msg, 2, 2, false));
  }
  // match entry 2, commit up to last new entry 2
  {
    Message msg;
    msg.set_type(MsgApp);
    msg.set_term(2);
    msg.set_logterm(2);
    msg.set_index(2);
    msg.set_commit(3);

    tests.push_back(tmp(msg, 2, 2, false));
  }
  {
    Message msg;
    msg.set_type(MsgApp);
    msg.set_term(2);
    msg.set_logterm(2);
    msg.set_index(2);
    msg.set_commit(4);

    tests.push_back(tmp(msg, 2, 2, false));
  }
  size_t i;
  for (i = 0; i < tests.size(); ++i) {
    tmp& t = tests[i];

    MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
    vector<Entry> entries;
    {
      Entry entry;
      entry.set_index(1);
      entry.set_term(1);
      entries.push_back(entry);
    }
    {
      Entry entry;
      entry.set_index(2);
      entry.set_term(2);
      entries.push_back(entry);
    }
    s->Append(entries);

    vector<uint64_t> peers;
    peers.push_back(1);
    raft *r = newTestRaft(1, peers, 10, 1, s);

    r->handleAppendEntries(t.m);
  
    EXPECT_EQ(r->raftLog_->lastIndex(), t.index) << "i: " << i;
    EXPECT_EQ(r->raftLog_->committed_, t.commit);

    MessageVec msgs;
    r->readMessages(&msgs);
    EXPECT_EQ((int)msgs.size(), 1);
    EXPECT_EQ(msgs[0]->reject(), t.reject);
  }
}

// TestHandleHeartbeat ensures that the follower commits to the commit in the message.
TEST(raftTests, TestHandleHeartbeat) {
  uint64_t commit = 2;
  struct tmp {
    Message m;
    uint64_t commit;

    tmp(Message m, uint64_t commit)
      : m(m), commit(commit) {}
  };

  vector<tmp> tests;
  {
    Message m;
    m.set_from(2);
    m.set_to(1);
    m.set_type(MsgHeartbeat);
    m.set_term(2);
    m.set_commit(commit + 1);
    tests.push_back(tmp(m, commit + 1));
  }
  // do not decrease commit
  {
    Message m;
    m.set_from(2);
    m.set_to(1);
    m.set_type(MsgHeartbeat);
    m.set_term(2);
    m.set_commit(commit - 1);
    tests.push_back(tmp(m, commit));
  }

  size_t i;
  for (i = 0; i < tests.size(); ++i) {
    tmp& t = tests[i];

    MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
    vector<Entry> entries;
    {
      Entry entry;
      entry.set_index(1);
      entry.set_term(1);
      entries.push_back(entry);
    }
    {
      Entry entry;
      entry.set_index(2);
      entry.set_term(2);
      entries.push_back(entry);
    }
    {
      Entry entry;
      entry.set_index(3);
      entry.set_term(3);
      entries.push_back(entry);
    }
    s->Append(entries);
    vector<uint64_t> peers;
    peers.push_back(1);
    peers.push_back(2);
    raft *r = newTestRaft(1, peers, 5, 1, s);

    r->becomeFollower(2, 2);
    r->raftLog_->commitTo(commit);
    r->handleHeartbeat(t.m);

    EXPECT_EQ(r->raftLog_->committed_, t.commit);
    vector<Message *> msgs;
    r->readMessages(&msgs);
    EXPECT_EQ((int)msgs.size(), 1);
    EXPECT_EQ(msgs[0]->type(), MsgHeartbeatResp);
  }
}

// TestHandleHeartbeatResp ensures that we re-send log entries when we get a heartbeat response.
TEST(raftTests, TestHandleHeartbeatResp) {
  MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
  vector<Entry> entries;
  {
    Entry entry;
    entry.set_index(1);
    entry.set_term(1);
    entries.push_back(entry);
  }
  {
    Entry entry;
    entry.set_index(2);
    entry.set_term(2);
    entries.push_back(entry);
  }
  {
    Entry entry;
    entry.set_index(3);
    entry.set_term(3);
    entries.push_back(entry);
  }
  s->Append(entries);
  vector<uint64_t> peers;
  peers.push_back(1);
  peers.push_back(2);
  raft *r = newTestRaft(1, peers, 5, 1, s);

  r->becomeCandidate();
  r->becomeLeader();

  r->raftLog_->commitTo(r->raftLog_->lastIndex());

  MessageVec msgs;
  // A heartbeat response from a node that is behind; re-send MsgApp
  {
    Message m;
    m.set_from(2);
    m.set_type(MsgHeartbeatResp);
    r->step(m);

    r->readMessages(&msgs);
    EXPECT_EQ((int)msgs.size(), 1);
    EXPECT_EQ(msgs[0]->type(), MsgApp);
  }

  // A second heartbeat response generates another MsgApp re-send
  {
    Message m;
    m.set_from(2);
    m.set_type(MsgHeartbeatResp);
    r->step(m);
    msgs.clear();
    r->readMessages(&msgs);
    EXPECT_EQ((int)msgs.size(), 1);
    EXPECT_EQ(msgs[0]->type(), MsgApp);
  }
  // Once we have an MsgAppResp, heartbeats no longer send MsgApp.
  {
    Message m;
    m.set_from(2);
    m.set_type(MsgAppResp);
    m.set_index(msgs[0]->index() + uint64_t(msgs[0]->entries_size()));
    r->step(m);
    MessageVec tmp_msgs;
    // Consume the message sent in response to MsgAppResp
    r->readMessages(&tmp_msgs);
  }

  {
    Message m;
    m.set_from(2);
    m.set_type(MsgHeartbeatResp);
    r->step(m);
    msgs.clear();
    r->readMessages(&msgs);
    EXPECT_EQ((int)msgs.size(), 0);
  }
}

// TestRaftFreesReadOnlyMem ensures raft will free read request from
// readOnly readIndexQueue and pendingReadIndex map.
// related issue: https://github.com/coreos/etcd/issues/7571
TEST(raftTests, TestRaftFreesReadOnlyMem) {
  MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
  vector<uint64_t> peers;
  peers.push_back(1);
  peers.push_back(2);
  raft *r = newTestRaft(1, peers, 5, 1, s);

  r->becomeCandidate();
  r->becomeLeader();

  r->raftLog_->commitTo(r->raftLog_->lastIndex());

  string ctx = "ctx";
  MessageVec msgs;

  // leader starts linearizable read request.
  // more info: raft dissertation 6.4, step 2.
  {
    Message m;
    m.set_from(2);
    m.set_type(MsgReadIndex);
    Entry *entry = m.add_entries();
    entry->set_data(ctx);
    r->step(m);

    r->readMessages(&msgs);
    EXPECT_EQ((int)msgs.size(), 1);
    EXPECT_EQ(msgs[0]->type(), MsgHeartbeat);
    EXPECT_EQ(msgs[0]->context(), ctx);
    EXPECT_EQ((int)r->readOnly_->readIndexQueue_.size(), 1);
    EXPECT_EQ((int)r->readOnly_->pendingReadIndex_.size(), 1);
    EXPECT_NE(r->readOnly_->pendingReadIndex_.find(ctx), r->readOnly_->pendingReadIndex_.end());
  }
  // heartbeat responses from majority of followers (1 in this case)
  // acknowledge the authority of the leader.
  // more info: raft dissertation 6.4, step 3.
  {
    Message m;
    m.set_from(2);
    m.set_type(MsgHeartbeatResp);
    m.set_context(ctx);
    r->step(m);

    EXPECT_EQ((int)r->readOnly_->readIndexQueue_.size(), 0);
    EXPECT_EQ((int)r->readOnly_->pendingReadIndex_.size(), 0);
    EXPECT_EQ(r->readOnly_->pendingReadIndex_.find(ctx), r->readOnly_->pendingReadIndex_.end());
  }
}

// TestMsgAppRespWaitReset verifies the resume behavior of a leader
// MsgAppResp.
TEST(raftTests, TestMsgAppRespWaitReset) {
  MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
  vector<uint64_t> peers;
  peers.push_back(1);
  peers.push_back(2);
  peers.push_back(3);
  raft *r = newTestRaft(1, peers, 5, 1, s);

  r->becomeCandidate();
  r->becomeLeader();

  MessageVec msgs;

  // The new leader has just emitted a new Term 4 entry; consume those messages
  // from the outgoing queue.
  r->bcastAppend();
  r->readMessages(&msgs);

  // Node 2 acks the first entry, making it committed.
  {
    Message msg;
    msg.set_from(2);
    msg.set_type(MsgAppResp);
    msg.set_index(1);
    r->step(msg);
  }
  EXPECT_EQ((int)r->raftLog_->committed_, 1);

  // Also consume the MsgApp messages that update Commit on the followers.
  r->readMessages(&msgs);

  // A new command is now proposed on node 1.
  {
    Message msg;
    msg.set_from(1);
    msg.set_type(MsgProp);
    //Entry *entry = msg.add_entries();
    msg.add_entries();
    r->step(msg);
  }

  // The command is broadcast to all nodes not in the wait state.
  // Node 2 left the wait state due to its MsgAppResp, but node 3 is still waiting.
  msgs.clear();
  r->readMessages(&msgs);
  EXPECT_EQ((int)msgs.size(), 1);
  Message *msg = msgs[0];
  EXPECT_FALSE(msg->type() != MsgApp || msg->to() != 2);
  EXPECT_FALSE(msg->entries_size() != 1 || msg->entries(0).index() != 2);

  // Now Node 3 acks the first entry. This releases the wait and entry 2 is sent.
  {
    Message tmp_msg;
    tmp_msg.set_from(3);
    tmp_msg.set_type(MsgAppResp);
    tmp_msg.set_index(1);
    r->step(tmp_msg);
  }
  msgs.clear();
  r->readMessages(&msgs);
  EXPECT_EQ((int)msgs.size(), 1);
  msg = msgs[0];
  EXPECT_FALSE(msg->type() != MsgApp || msg->to() != 3);
  EXPECT_FALSE(msg->entries_size() != 1 || msg->entries(0).index() != 2);
}

void testRecvMsgVote(MessageType type) {
  struct tmp {
    StateType state;
    uint64_t i, term;
    uint64_t voteFor;
    bool reject;

    tmp(StateType t, uint64_t i, uint64_t term, uint64_t vote, bool reject)
      : state(t), i(i), term(term), voteFor(vote), reject(reject) {}
  };

  vector<tmp> tests;
  
  tests.push_back(tmp(StateFollower, 0, 0, kEmptyPeerId, true));
  tests.push_back(tmp(StateFollower, 0, 1, kEmptyPeerId, true));
  tests.push_back(tmp(StateFollower, 0, 2, kEmptyPeerId, true));
  tests.push_back(tmp(StateFollower, 0, 3, kEmptyPeerId, false));

  tests.push_back(tmp(StateFollower, 1, 0, kEmptyPeerId, true));
  tests.push_back(tmp(StateFollower, 1, 1, kEmptyPeerId, true));
  tests.push_back(tmp(StateFollower, 1, 2, kEmptyPeerId, true));
  tests.push_back(tmp(StateFollower, 1, 3, kEmptyPeerId, false));

  tests.push_back(tmp(StateFollower, 2, 0, kEmptyPeerId, true));
  tests.push_back(tmp(StateFollower, 2, 1, kEmptyPeerId, true));
  tests.push_back(tmp(StateFollower, 2, 2, kEmptyPeerId, false));
  tests.push_back(tmp(StateFollower, 2, 3, kEmptyPeerId, false));

  tests.push_back(tmp(StateFollower, 3, 0, kEmptyPeerId, true));
  tests.push_back(tmp(StateFollower, 3, 1, kEmptyPeerId, true));
  tests.push_back(tmp(StateFollower, 3, 2, kEmptyPeerId, false));
  tests.push_back(tmp(StateFollower, 3, 3, kEmptyPeerId, false));

  tests.push_back(tmp(StateFollower, 3, 2, 2, false));
  tests.push_back(tmp(StateFollower, 3, 2, 1, true));

  tests.push_back(tmp(StateLeader, 3, 3, 1, true));
  tests.push_back(tmp(StatePreCandidate, 3, 3, 1, true));
  tests.push_back(tmp(StateCandidate, 3, 3, 1, true));

  size_t i;
  for (i = 0; i < tests.size(); ++i) {
    tmp &t = tests[i];
    MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
    vector<uint64_t> peers;
    peers.push_back(1);
    raft *r = newTestRaft(1, peers, 10, 1, s);

    r->state_ = t.state;
    switch (t.state) {
    case StateFollower:
      r->stateStepFunc_ = stepFollower;
      break;
    case StateCandidate:
      r->stateStepFunc_ = stepCandidate;
      break;
    case StateLeader:
      r->stateStepFunc_ = stepLeader;
      break;
    default:
      break;
    }
    r->vote_ = t.voteFor;  

    s = new MemoryStorage(&kDefaultLogger); 
    EntryVec entries;

    entries.push_back(Entry());
    {
      Entry entry;
      entry.set_term(2);
      entry.set_index(1);
      entries.push_back(entry);
    }
    {
      Entry entry;
      entry.set_term(2);
      entry.set_index(2);
      entries.push_back(entry);
    }
    s->entries_.clear();
    s->entries_.insert(s->entries_.end(), entries.begin(), entries.end());
    r->raftLog_ = new raftLog(s, &kDefaultLogger);
    r->raftLog_->unstable_.offset_ = 3;

    {
      Message msg;
      msg.set_type(type);
      msg.set_from(2);
      msg.set_index(t.i);
      msg.set_logterm(t.term);
      r->step(msg);
    }

    MessageVec msgs;
    r->readMessages(&msgs);

    EXPECT_EQ((int)msgs.size(), 1);
    EXPECT_EQ(msgs[0]->type(), voteRespMsgType(type));
    EXPECT_EQ(msgs[0]->reject(), t.reject) << "i: " << i;
  }
}

TEST(raftTests, TestRecvMsgVote) {
  testRecvMsgVote(MsgVote); 
}

// TODO
TEST(raftTests, TestStateTransition) {
  struct tmp {
  };
}

TEST(raftTests, TestAllServerStepdown) {
  struct tmp {
    StateType state, wstate;
    uint64_t term, index;
    
    tmp(StateType s, StateType ws, uint64_t t, uint64_t i)
      : state(s), wstate(ws), term(t), index(i) {
    }
  };

  vector<tmp> tests;
  tests.push_back(tmp(StateFollower, StateFollower, 3, 0));
  tests.push_back(tmp(StatePreCandidate, StateFollower, 3, 0));
  tests.push_back(tmp(StateCandidate, StateFollower, 3, 0));
  tests.push_back(tmp(StateLeader, StateFollower, 3, 1));

  vector<MessageType> types;
  types.push_back(MsgVote);
  types.push_back(MsgApp);
  uint64_t term = 3;

  size_t i;
  for (i = 0; i < tests.size(); ++i) {
    tmp &t = tests[i];
    MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
    vector<uint64_t> peers;
    peers.push_back(1);
    peers.push_back(2);
    peers.push_back(3);
    raft *r = newTestRaft(1, peers, 10, 1, s);

    switch (t.state) {
    case StateFollower:
      r->becomeFollower(1, kEmptyPeerId);
      break;
    case StatePreCandidate:
      r->becomePreCandidate();
      break;
    case StateCandidate:
      r->becomeCandidate();
      break;
    case StateLeader:
      r->becomeCandidate();
      r->becomeLeader();
      break;
    default:
      break;      
    }

    size_t j;
    for (j = 0; j < types.size(); ++j) {
      MessageType type = types[j];
      Message msg;
      msg.set_from(2);
      msg.set_type(type);
      msg.set_term(term);
      msg.set_logterm(term);
      r->step(msg);
      
      EXPECT_EQ(r->state_, t.wstate);
      EXPECT_EQ(r->term_, t.term);
      EXPECT_EQ(r->raftLog_->lastIndex(), t.index);
      EntryVec entries;
      r->raftLog_->allEntries(&entries);
      EXPECT_EQ(entries.size(), t.index);

      uint64_t leader = 2;
      if (type == MsgVote) {
        leader = kEmptyPeerId;
      }
      EXPECT_EQ(r->leader_, leader);
    }
  }
}

TEST(raftTests, TestLeaderStepdownWhenQuorumActive) {
  MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
  vector<uint64_t> peers;
  peers.push_back(1);
  peers.push_back(2);
  peers.push_back(3);
  raft *r = newTestRaft(1, peers, 5, 1, s);

  r->checkQuorum_ = true;
  
  r->becomeCandidate();
  r->becomeLeader();

  int i;
  for (i = 0; i < r->electionTimeout_ + 1; ++i) {
    Message msg;
    msg.set_from(2);
    msg.set_type(MsgHeartbeatResp);
    msg.set_term(r->term_);
    r->step(msg);
    r->tick();
  }

  EXPECT_EQ(r->state_, StateLeader);
}

TEST(raftTests, TestLeaderStepdownWhenQuorumLost) {
  MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
  vector<uint64_t> peers;
  peers.push_back(1);
  peers.push_back(2);
  peers.push_back(3);
  raft *r = newTestRaft(1, peers, 5, 1, s);

  r->checkQuorum_ = true;
  
  r->becomeCandidate();
  r->becomeLeader();

  int i;
  for (i = 0; i < r->electionTimeout_ + 1; ++i) {
    r->tick();
  }

  EXPECT_EQ(r->state_, StateFollower);
}

TEST(raftTests, TestLeaderSupersedingWithCheckQuorum) {
  raft *a, *b, *c;
  vector<stateMachine*> peers;

  {
    MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
    vector<uint64_t> tmp_peers;
    tmp_peers.push_back(1);
    tmp_peers.push_back(2);
    tmp_peers.push_back(3);
    a = newTestRaft(1, tmp_peers, 10, 1, s);
    a->checkQuorum_ = true;
  }
  {
    MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
    vector<uint64_t> tmp_peers;
    tmp_peers.push_back(1);
    tmp_peers.push_back(2);
    tmp_peers.push_back(3);
    b = newTestRaft(2, tmp_peers, 10, 1, s);
    b->checkQuorum_ = true;
  }
  {
    MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
    vector<uint64_t> tmp_peers;
    tmp_peers.push_back(1);
    tmp_peers.push_back(2);
    tmp_peers.push_back(3);
    c = newTestRaft(3, tmp_peers, 10, 1, s);
    c->checkQuorum_ = true;
  }
  peers.push_back(new raftStateMachine(a));
  peers.push_back(new raftStateMachine(b));
  peers.push_back(new raftStateMachine(c));
  network *net = newNetwork(peers);
  
  b->randomizedElectionTimeout_ = b->electionTimeout_ + 1;

  int i;
  for (i = 0; i < b->electionTimeout_; ++i) {
    b->tick();
  }

  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgHup);
    msgs.push_back(msg);
    net->send(&msgs);
  }  

  EXPECT_EQ(a->state_, StateLeader);
  EXPECT_EQ(c->state_, StateFollower);

  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(3);
    msg.set_to(3);
    msg.set_type(MsgHup);
    msgs.push_back(msg);
    net->send(&msgs);
  }  

  // Peer b rejected c's vote since its electionElapsed had not reached to electionTimeout
  EXPECT_EQ(c->state_, StateCandidate);

  // Letting b's electionElapsed reach to electionTimeout
  for (i = 0; i < b->electionTimeout_; ++i) {
    b->tick();
  }
  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(3);
    msg.set_to(3);
    msg.set_type(MsgHup);
    msgs.push_back(msg);
    net->send(&msgs);
  }  
  EXPECT_EQ(c->state_, StateLeader);
}

TEST(raftTests, TestLeaderElectionWithCheckQuorum) {
  raft *a, *b, *c;
  vector<stateMachine*> peers;

  {
    MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
    vector<uint64_t> tmp_peers;
    tmp_peers.push_back(1);
    tmp_peers.push_back(2);
    tmp_peers.push_back(3);
    a = newTestRaft(1, tmp_peers, 10, 1, s);
    a->checkQuorum_ = true;
  }
  {
    MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
    vector<uint64_t> tmp_peers;
    tmp_peers.push_back(1);
    tmp_peers.push_back(2);
    tmp_peers.push_back(3);
    b = newTestRaft(2, tmp_peers, 10, 1, s);
    b->checkQuorum_ = true;
  }
  {
    MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
    vector<uint64_t> tmp_peers;
    tmp_peers.push_back(1);
    tmp_peers.push_back(2);
    tmp_peers.push_back(3);
    c = newTestRaft(3, tmp_peers, 10, 1, s);
    c->checkQuorum_ = true;
  }
  peers.push_back(new raftStateMachine(a));
  peers.push_back(new raftStateMachine(b));
  peers.push_back(new raftStateMachine(c));
  network *net = newNetwork(peers);
  
  a->randomizedElectionTimeout_ = a->electionTimeout_ + 1;
  b->randomizedElectionTimeout_ = b->electionTimeout_ + 2;

  int i;
  // Immediately after creation, votes are cast regardless of the
  // election timeout.
  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgHup);
    msgs.push_back(msg);
    net->send(&msgs);
  }  

  EXPECT_EQ(a->state_, StateLeader);
  EXPECT_EQ(c->state_, StateFollower);

  // need to reset randomizedElectionTimeout larger than electionTimeout again,
  // because the value might be reset to electionTimeout since the last state changes
  a->randomizedElectionTimeout_ = a->electionTimeout_ + 1;
  b->randomizedElectionTimeout_ = b->electionTimeout_ + 2;

  for (i = 0; i < a->electionTimeout_; ++i) {
    a->tick();
  }
  for (i = 0; i < b->electionTimeout_; ++i) {
    b->tick();
  }
  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(3);
    msg.set_to(3);
    msg.set_type(MsgHup);
    msgs.push_back(msg);
    net->send(&msgs);
  }  
  EXPECT_EQ(a->state_, StateFollower);
  EXPECT_EQ(c->state_, StateLeader);
}

// TestFreeStuckCandidateWithCheckQuorum ensures that a candidate with a higher term
// can disrupt the leader even if the leader still "officially" holds the lease, The
// leader is expected to step down and adopt the candidate's term
TEST(raftTests, TestFreeStuckCandidateWithCheckQuorum) {
  raft *a, *b, *c;
  vector<stateMachine*> peers;

  {
    MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
    vector<uint64_t> tmp_peers;
    tmp_peers.push_back(1);
    tmp_peers.push_back(2);
    tmp_peers.push_back(3);
    a = newTestRaft(1, tmp_peers, 10, 1, s);
    a->checkQuorum_ = true;
  }
  {
    MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
    vector<uint64_t> tmp_peers;
    tmp_peers.push_back(1);
    tmp_peers.push_back(2);
    tmp_peers.push_back(3);
    b = newTestRaft(2, tmp_peers, 10, 1, s);
    b->checkQuorum_ = true;
  }
  {
    MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
    vector<uint64_t> tmp_peers;
    tmp_peers.push_back(1);
    tmp_peers.push_back(2);
    tmp_peers.push_back(3);
    c = newTestRaft(3, tmp_peers, 10, 1, s);
    c->checkQuorum_ = true;
  }
  peers.push_back(new raftStateMachine(a));
  peers.push_back(new raftStateMachine(b));
  peers.push_back(new raftStateMachine(c));
  network *net = newNetwork(peers);

  b->randomizedElectionTimeout_ = b->electionTimeout_ + 2;
  int i;
  for (i = 0; i < b->electionTimeout_; ++i) {
    b->tick();
  }
  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgHup);
    msgs.push_back(msg);
    net->send(&msgs);
  }  

  net->isolate(1);

  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(3);
    msg.set_to(3);
    msg.set_type(MsgHup);
    msgs.push_back(msg);
    net->send(&msgs);
  }  
  EXPECT_EQ(b->state_, StateFollower);
  EXPECT_EQ(c->state_, StateCandidate);
  EXPECT_EQ(c->term_, b->term_ + 1);

  // Vote again for safety
  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(3);
    msg.set_to(3);
    msg.set_type(MsgHup);
    msgs.push_back(msg);
    net->send(&msgs);
  }  
  EXPECT_EQ(b->state_, StateFollower);
  EXPECT_EQ(c->state_, StateCandidate);
  EXPECT_EQ(c->term_, b->term_ + 2);

  net->recover();
  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(1);
    msg.set_to(3);
    msg.set_type(MsgHeartbeat);
    msgs.push_back(msg);
    net->send(&msgs);
  }  
  // Disrupt the leader so that the stuck peer is freed
  EXPECT_EQ(a->state_, StateFollower);
  EXPECT_EQ(a->term_, c->term_);
}

TEST(raftTests, TestNonPromotableVoterWithCheckQuorum) {
  raft *a, *b;
  vector<stateMachine*> peers;

  {
    MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
    vector<uint64_t> tmp_peers;
    tmp_peers.push_back(1);
    tmp_peers.push_back(2);
    a = newTestRaft(1, tmp_peers, 10, 1, s);
    a->checkQuorum_ = true;
  }
  {
    MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
    vector<uint64_t> tmp_peers;
    tmp_peers.push_back(1);
    b = newTestRaft(2, tmp_peers, 10, 1, s);
    b->checkQuorum_ = true;
  }
  peers.push_back(new raftStateMachine(a));
  peers.push_back(new raftStateMachine(b));
  network *net = newNetwork(peers);

  b->randomizedElectionTimeout_ = b->electionTimeout_ + 1;
  // Need to remove 2 again to make it a non-promotable node since newNetwork overwritten some internal states
  b->delProgress(2);
  EXPECT_FALSE(b->promotable());
  int i;
  for (i = 0; i < b->electionTimeout_; ++i) {
    b->tick();
  }
  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgHup);
    msgs.push_back(msg);
    net->send(&msgs);
  }  

  EXPECT_EQ(a->state_, StateLeader);
  EXPECT_EQ(b->state_, StateFollower);
  EXPECT_EQ((int)b->leader_, 1);
}

TEST(raftTests, TestReadOnlyOptionSafe) {
  raft *a, *b, *c;
  vector<stateMachine*> peers;

  {
    MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
    vector<uint64_t> tmp_peers;
    tmp_peers.push_back(1);
    tmp_peers.push_back(2);
    tmp_peers.push_back(3);
    a = newTestRaft(1, tmp_peers, 10, 1, s);
    a->checkQuorum_ = true;
  }
  {
    MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
    vector<uint64_t> tmp_peers;
    tmp_peers.push_back(1);
    tmp_peers.push_back(2);
    tmp_peers.push_back(3);
    b = newTestRaft(2, tmp_peers, 10, 1, s);
    b->checkQuorum_ = true;
  }
  {
    MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
    vector<uint64_t> tmp_peers;
    tmp_peers.push_back(1);
    tmp_peers.push_back(2);
    tmp_peers.push_back(3);
    c = newTestRaft(3, tmp_peers, 10, 1, s);
    c->checkQuorum_ = true;
  }
  peers.push_back(new raftStateMachine(a));
  peers.push_back(new raftStateMachine(b));
  peers.push_back(new raftStateMachine(c));
  network *net = newNetwork(peers);

  b->randomizedElectionTimeout_ = b->electionTimeout_ + 2;
  int i;
  for (i = 0; i < b->electionTimeout_; ++i) {
    b->tick();
  }
  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgHup);
    msgs.push_back(msg);
    net->send(&msgs);
  }  

  EXPECT_EQ(a->state_, StateLeader);
  struct tmp {
    raft *r;
    int proposals;
    uint64_t wri;
    string ctx;

    tmp(raft *r, int proposals, uint64_t wri, string ctx)
      : r(r), proposals(proposals), wri(wri), ctx(ctx) {}
  };

  vector<tmp> tests;
  tests.push_back(tmp(a, 10, 11, "ctx1"));
  tests.push_back(tmp(b, 10, 21, "ctx2"));
  tests.push_back(tmp(c, 10, 31, "ctx3"));
  tests.push_back(tmp(a, 10, 41, "ctx4"));
  tests.push_back(tmp(b, 10, 51, "ctx5"));
  tests.push_back(tmp(c, 10, 61, "ctx6"));

  for (i = 0; i < (int)tests.size(); ++i) {
    tmp& t = tests[i];
    int j;
    for (j = 0; j < t.proposals; ++j) {
      vector<Message> msgs;
      Message msg;
      msg.set_from(1);
      msg.set_to(1);
      msg.set_type(MsgProp);
      msg.add_entries();
      msgs.push_back(msg);
      net->send(&msgs);
    }
    {
      vector<Message> msgs;
      Message msg;
      msg.set_from(t.r->id_);
      msg.set_to(t.r->id_);
      msg.set_type(MsgReadIndex);
      Entry *entry = msg.add_entries();
      entry->set_data(t.ctx);
      msgs.push_back(msg);
      net->send(&msgs);
    }

    raft *r = t.r;
    EXPECT_FALSE(r->readStates_.size() == 0);
    ReadState* state = r->readStates_[0];
    EXPECT_EQ(state->index_, t.wri);
    EXPECT_EQ(state->requestCtx_, t.ctx);
    r->readStates_.clear();
  }
}

TEST(raftTests, TestReadOnlyOptionLease) {
  vector<uint64_t> peers;
  vector<stateMachine*> sts;
  peers.push_back(1);
  peers.push_back(2);
  peers.push_back(3);

  raft *a, *b, *c;
  {
    MemoryStorage *s = new MemoryStorage(&kDefaultLogger);

    a = newTestRaft(1, peers, 10, 1, s);
    a->readOnly_->option_ = ReadOnlyLeaseBased;
    a->checkQuorum_ = true;
    sts.push_back(new raftStateMachine(a));
  }
  {
    MemoryStorage *s = new MemoryStorage(&kDefaultLogger);

    b = newTestRaft(2, peers, 10, 1, s);
    b->readOnly_->option_ = ReadOnlyLeaseBased;
    b->checkQuorum_ = true;
    sts.push_back(new raftStateMachine(b));
  }
  {
    MemoryStorage *s = new MemoryStorage(&kDefaultLogger);

    c = newTestRaft(3, peers, 10, 1, s);
    c->readOnly_->option_ = ReadOnlyLeaseBased;
    c->checkQuorum_ = true;
    sts.push_back(new raftStateMachine(c));
  }
  
  network *net = newNetwork(sts);
  b->randomizedElectionTimeout_ = b->electionTimeout_ + 1;
  int i;
  for (i = 0; i < b->electionTimeout_; ++i) {
    b->tick();
  }

  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgHup);
    msgs.push_back(msg);
    net->send(&msgs);
  }
  EXPECT_EQ(a->state_, StateLeader);
  
  struct tmp {
    raft *r;
    int proposals;
    uint64_t wri;
    string wctx;

    tmp(raft *r, int proposals, uint64_t wri, string ctx)
      : r(r), proposals(proposals), wri(wri), wctx(ctx){
    }
  };

  vector<tmp> tests;
  tests.push_back(tmp(a, 10, 11, "ctx1"));
  tests.push_back(tmp(b, 10, 21, "ctx2"));
  tests.push_back(tmp(c, 10, 31, "ctx3"));
  tests.push_back(tmp(a, 10, 41, "ctx4"));
  tests.push_back(tmp(b, 10, 51, "ctx5"));
  tests.push_back(tmp(c, 10, 61, "ctx6"));

  for (i = 0; i < (int)tests.size(); ++i) {
    tmp &t = tests[i];
    int j;
    for (j = 0; j < t.proposals; ++j) {
      vector<Message> msgs;
      Message msg;
      msg.set_from(1);
      msg.set_to(1);
      msg.set_type(MsgProp);
      msg.add_entries();
      msgs.push_back(msg);
      net->send(&msgs);
    }
    {
      vector<Message> msgs;
      Message msg;
      msg.set_from(t.r->id_);
      msg.set_to(t.r->id_);
      msg.set_type(MsgReadIndex);
      msg.add_entries()->set_data(t.wctx);
      msgs.push_back(msg);
      net->send(&msgs);
    }

    raft *r = t.r;
    ReadState *s = r->readStates_[0];
    EXPECT_EQ(s->index_, t.wri);
    EXPECT_EQ(s->requestCtx_, t.wctx);

    r->readStates_.clear();
  }
}

TEST(raftTests, TestReadOnlyOptionLeaseWithoutCheckQuorum) {
  vector<uint64_t> peers;
  vector<stateMachine*> sts;
  peers.push_back(1);
  peers.push_back(2);
  peers.push_back(3);

  raft *a, *b, *c;
  {
    MemoryStorage *s = new MemoryStorage(&kDefaultLogger);

    a = newTestRaft(1, peers, 10, 1, s);
    a->readOnly_->option_ = ReadOnlyLeaseBased;
    sts.push_back(new raftStateMachine(a));
  }
  {
    MemoryStorage *s = new MemoryStorage(&kDefaultLogger);

    b = newTestRaft(2, peers, 10, 1, s);
    b->readOnly_->option_ = ReadOnlyLeaseBased;
    sts.push_back(new raftStateMachine(b));
  }
  {
    MemoryStorage *s = new MemoryStorage(&kDefaultLogger);

    c = newTestRaft(3, peers, 10, 1, s);
    c->readOnly_->option_ = ReadOnlyLeaseBased;
    sts.push_back(new raftStateMachine(c));
  }
  
  network *net = newNetwork(sts);
  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgHup);
    msgs.push_back(msg);
    net->send(&msgs);
  }

  string ctx = "ctx1";

  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(2);
    msg.set_to(2);
    msg.set_type(MsgReadIndex);
    msg.add_entries()->set_data(ctx);
    msgs.push_back(msg);
    net->send(&msgs);
  }
  ReadState *s = b->readStates_[0];
  EXPECT_EQ(s->index_, kEmptyPeerId);
  EXPECT_EQ(s->requestCtx_, ctx);
}

// TestReadOnlyForNewLeader ensures that a leader only accepts MsgReadIndex message
// when it commits at least one log entry at it term.
// TODO
TEST(raftTests, TestReadOnlyForNewLeader) {
  vector<uint64_t> peers;
  vector<stateMachine*> sts;
  peers.push_back(1);
  peers.push_back(2);
  peers.push_back(3);

  raft *a, *b, *c;
  {
    MemoryStorage *s = new MemoryStorage(&kDefaultLogger);

    vector<Entry> entries;
    
    entries.push_back(Entry());
    
    Entry entry;
    entry.set_index(1);
    entry.set_term(1);
    entries.push_back(entry);

    entry.set_index(2);
    entry.set_term(1);
    entries.push_back(entry);
    s->entries_ = entries;

    s->hardState_.set_commit(1);
    s->hardState_.set_term(1);

    Config *tmp_c = newTestConfig(1, peers, 10, 1, s);
    tmp_c->applied = 1;
    a = newRaft(tmp_c);
    sts.push_back(new raftStateMachine(a));
  }
  {
    MemoryStorage *s = new MemoryStorage(&kDefaultLogger);

    vector<Entry> entries;
    
    Entry entry;
    entry.set_index(1);
    entry.set_term(1);
    entries.push_back(entry);

    entry.set_index(2);
    entry.set_term(1);
    entries.push_back(entry);
    s->entries_ = entries;

    s->hardState_.set_commit(2);
    s->hardState_.set_term(1);

    Config *tmp_c = newTestConfig(2, peers, 10, 1, s);
    tmp_c->applied = 2;
    b = newRaft(tmp_c);
    sts.push_back(new raftStateMachine(b));
  }
  {
    MemoryStorage *s = new MemoryStorage(&kDefaultLogger);

    vector<Entry> entries;
    
    Entry entry;
    entry.set_index(1);
    entry.set_term(1);
    entries.push_back(entry);

    entry.set_index(2);
    entry.set_term(1);
    entries.push_back(entry);
    s->entries_ = entries;

    s->hardState_.set_commit(2);
    s->hardState_.set_term(1);

    Config *cf = newTestConfig(2, peers, 10, 1, s);
    cf->applied = 2;
    c = newRaft(cf);
    sts.push_back(new raftStateMachine(c));
  }
  
  network *net = newNetwork(sts);

  // Drop MsgApp to forbid peer a to commit any log entry at its term after it becomes leader.
  net->ignore(MsgApp);
  // Force peer a to become leader.
  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgHup);
    msgs.push_back(msg);
    net->send(&msgs);
  }

  EXPECT_EQ(a->state_, StateLeader);

  // Ensure peer a drops read only request.
  uint64_t index = 4;
  string ctx = "ctx";
  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgReadIndex);
    Entry *entry = msg.add_entries();
    entry->set_data(ctx);
    msgs.push_back(msg);
    net->send(&msgs);
  }
  EXPECT_EQ((int)a->readStates_.size(), 0);

  net->recover();

  // Force peer a to commit a log entry at its term
  int i;
  for (i = 0; i < a->heartbeatTimeout_; ++i) {
    a->tick();
  }

  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgProp);
    msg.add_entries();
    msgs.push_back(msg);
    net->send(&msgs);
  }
  EXPECT_EQ((int)a->raftLog_->committed_, 4);

  uint64_t term;
  int err = a->raftLog_->term(a->raftLog_->committed_, &term);
  uint64_t lastLogTerm = a->raftLog_->zeroTermOnErrCompacted(term, err); 
  EXPECT_EQ(lastLogTerm, a->term_);

  // Ensure peer a accepts read only request after it commits a entry at its term.
  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgReadIndex);
    msg.add_entries()->set_data(ctx);
    msgs.push_back(msg);
    net->send(&msgs);
  }
  EXPECT_EQ((int)a->readStates_.size(), 1);
  ReadState *rs = a->readStates_[0];
  EXPECT_EQ(rs->index_, index);
  EXPECT_EQ(rs->requestCtx_, ctx);
}

TEST(raftTests, TestLeaderAppResp) {
  // initial progress: match = 0; next = 3
  struct tmp {
    uint64_t index;
    bool reject;
    // progress
    uint64_t match;
    uint64_t next;
    // message
    int msgNum;
    uint64_t windex;
    uint64_t wcommit;
    
  } tests[] = {
    // stale resp; no replies
    {.index=3, .reject=true,  .match=0, .next=3, .msgNum=0, .windex=0, .wcommit=0},
    // denied resp; leader does not commit; decrease next and send probing msg
    {.index=2, .reject=true,  .match=0, .next=2, .msgNum=1, .windex=1, .wcommit=0},
    // accept resp; leader commits; broadcast with commit index
    {.index=2, .reject=false, .match=2, .next=4, .msgNum=2, .windex=2, .wcommit=2},
    // ignore heartbeat replies
    {.index=0, .reject=false, .match=0, .next=3, .msgNum=0, .windex=0, .wcommit=0},
  };

  int i;
  for (i = 0; i < (int)(sizeof(tests)/sizeof(tests[0])); ++i) {
 		// sm term is 1 after it becomes the leader.
		// thus the last log term must be 1 to be committed.
    tmp &t = tests[i];
    MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
    vector<uint64_t> peers;
    peers.push_back(1);
    peers.push_back(2);
    peers.push_back(3);
    raft* r = newTestRaft(1, peers, 10, 1, s);

    {
      MemoryStorage *tmp_s = new MemoryStorage(&kDefaultLogger);
      EntryVec entries;
      entries.push_back(Entry());

      Entry entry;
      entry.set_index(1);
      entry.set_term(0);
      entries.push_back(entry);

      entry.set_index(2);
      entry.set_term(1);
      entries.push_back(entry);

      tmp_s->entries_ = entries;
      r->raftLog_ = newLog(tmp_s, &kDefaultLogger);
      r->raftLog_->unstable_.offset_ = 3;
    }

    r->becomeCandidate();
    r->becomeLeader();
    MessageVec msgs;
    r->readMessages(&msgs);

    {
      Message msg;
      msg.set_from(2);
      msg.set_type(MsgAppResp);
      msg.set_index(t.index);
      msg.set_term(r->term_);
      msg.set_reject(t.reject);
      msg.set_rejecthint(t.index);
      r->step(msg);
    }

    Progress *p = r->progressMap_[2];
    EXPECT_EQ(p->match_, t.match);
    EXPECT_EQ(p->next_, t.next);

    r->readMessages(&msgs);

    EXPECT_EQ((int)msgs.size(), t.msgNum);
    size_t j;
    for (j = 0; j < msgs.size(); ++j) {
      Message *msg = msgs[j];
      EXPECT_EQ(msg->index(), t.windex);
      EXPECT_EQ(msg->commit(), t.wcommit);
    }
  }

}

// When the leader receives a heartbeat tick, it should
// send a MsgApp with m.Index = 0, m.LogTerm=0 and empty entries.
TEST(raftTests, TestBcastBeat) {
  uint64_t offset = 1000;
  // make a state machine with log.offset = 1000
  Snapshot ss;
  ss.mutable_metadata()->set_index(offset);
  ss.mutable_metadata()->set_term(1);
  ss.mutable_metadata()->mutable_conf_state()->add_nodes(1);
  ss.mutable_metadata()->mutable_conf_state()->add_nodes(2);
  ss.mutable_metadata()->mutable_conf_state()->add_nodes(3);
  MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
  vector<uint64_t> peers;
  s->ApplySnapshot(ss);
  raft *r = newTestRaft(1, peers, 10, 1, s);
  r->term_ = 1;

  r->becomeCandidate();
  r->becomeLeader();

  EntryVec entries;
  size_t i;
  for (i = 0; i < 10; ++i) {
    Entry entry;
    entry.set_index(i + 1);
    entries.push_back(entry);
  }
  r->appendEntry(&entries);
  // slow follower
  r->progressMap_[2]->match_ = 5;
  r->progressMap_[2]->next_ = 6;
  // normal follower
  r->progressMap_[3]->match_ = r->raftLog_->lastIndex();
  r->progressMap_[3]->next_ = r->raftLog_->lastIndex() + 1;

  {
    Message msg;
    msg.set_type(MsgBeat);
    r->step(msg);
  }

  MessageVec msgs;
  r->readMessages(&msgs);
  
  EXPECT_EQ((int)msgs.size(), 2);
  map<uint64_t, uint64_t> wantCommitMap;
  wantCommitMap[2] = min(r->raftLog_->committed_, r->progressMap_[2]->match_);
  wantCommitMap[3] = min(r->raftLog_->committed_, r->progressMap_[3]->match_);

  for (i = 0; i < msgs.size(); ++i) {
    Message *msg = msgs[i];
    EXPECT_EQ(msg->type(), MsgHeartbeat);
    EXPECT_EQ((int)msg->index(), 0);
    EXPECT_EQ((int)msg->logterm(), 0);
    EXPECT_NE((int)wantCommitMap[msg->to()], 0);
    EXPECT_EQ(msg->commit(), wantCommitMap[msg->to()]);
    EXPECT_EQ((int)msg->entries_size(), 0);
  }
}

// tests the output of the state machine when receiving MsgBeat
TEST(raftTests, TestRecvMsgBeat) {
  struct tmp {
    StateType state;
    int msg;

    tmp(StateType s, int msg)
      : state(s), msg(msg) {
    }
  };

  vector<tmp> tests;
  tests.push_back(tmp(StateLeader, 2));
  // candidate and follower should ignore MsgBeat
  tests.push_back(tmp(StateCandidate, 0));
  tests.push_back(tmp(StateFollower, 0));

  size_t i;
  for (i = 0; i < tests.size(); ++i) {
    tmp& t = tests[i];
    MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
    vector<uint64_t> peers;
    peers.push_back(1);
    peers.push_back(2);
    peers.push_back(3);
    raft *r = newTestRaft(1, peers, 10, 1, s);

    EntryVec entries;
    entries.push_back(Entry());

    Entry entry;
    entry.set_index(1);
    entry.set_term(0);
    entries.push_back(entry);

    entry.set_index(2);
    entry.set_term(1);
    entries.push_back(entry);
    s->entries_ = entries;
    r->raftLog_ = newLog(s, &kDefaultLogger);
    r->term_ = 1;
    r->state_ = t.state;
    switch (t.state) {
    case StateFollower:
      r->stateStepFunc_ = stepFollower;
      break;
    case StateCandidate:
      r->stateStepFunc_ = stepCandidate;
      break;
    case StateLeader:
      r->stateStepFunc_ = stepLeader;
      break;
    default:
      break;
    }

    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgBeat);
    r->step(msg);

    MessageVec msgs;
    r->readMessages(&msgs);

    EXPECT_EQ((int)msgs.size(), t.msg) << "i: " << i;

    size_t j;
    for (j = 0; j < msgs.size(); ++j) {
      EXPECT_EQ(msgs[j]->type(), MsgHeartbeat);
    }
  }
}

TEST(raftTests, TestLeaderIncreaseNext) {
  EntryVec prevEntries;
  {
    Entry entry;

    entry.set_term(1);
    entry.set_index(1);
    prevEntries.push_back(entry);

    entry.set_term(1);
    entry.set_index(2);
    prevEntries.push_back(entry);

    entry.set_term(1);
    entry.set_index(3);
    prevEntries.push_back(entry);
  }
  struct tmp {
    ProgressState state;
    uint64_t next, wnext;
    tmp(ProgressState s, uint64_t n, uint64_t wn)
      : state(s), next(n), wnext(wn) {
    }
  };
  vector<tmp> tests;
	// state replicate, optimistically increase next
	// previous entries + noop entry + propose + 1
  tests.push_back(tmp(ProgressStateReplicate, 2, prevEntries.size() + 1 + 1 + 1));
	// state probe, not optimistically increase next
  tests.push_back(tmp(ProgressStateProbe, 2, 2));

	size_t i;
	for (i = 0; i < tests.size(); ++i) {
		tmp& t = tests[i];

		vector<uint64_t> peers;
		peers.push_back(1);
		peers.push_back(2);
		Storage *s = new MemoryStorage(&kDefaultLogger);
		raft *r = newTestRaft(1, peers, 10, 1, s);
		r->raftLog_->append(prevEntries);
		r->becomeCandidate();
		r->becomeLeader();
		r->progressMap_[2]->state_ = t.state;
		r->progressMap_[2]->next_ = t.next;

		Message msg;
		msg.set_from(1);
		msg.set_to(1);
		msg.set_type(MsgProp);
		msg.add_entries()->set_data("somedata");
		r->step(msg);

		EXPECT_EQ(r->progressMap_[2]->next_, t.wnext);
	}
}

TEST(raftTests, TestSendAppendForProgressProbe) {
	vector<uint64_t> peers;
	peers.push_back(1);
	peers.push_back(2);
	Storage *s = new MemoryStorage(&kDefaultLogger);
	raft *r = newTestRaft(1, peers, 10, 1, s);
	r->becomeCandidate();
	r->becomeLeader();

	MessageVec msgs;
	r->readMessages(&msgs);
	r->progressMap_[2]->becomeProbe();

	// each round is a heartbeat
	size_t i;
	for (i = 0; i < 3; ++i) {
		if (i == 0) {
			// we expect that raft will only send out one msgAPP on the first
			// loop. After that, the follower is paused until a heartbeat response is
			// received.
			EntryVec entries;
			Entry entry;
			entry.set_data("somedata");
			entries.push_back(entry);
			r->appendEntry(&entries);
			r->sendAppend(2);
			r->readMessages(&msgs);
			EXPECT_EQ((int)msgs.size(), 1);
			EXPECT_EQ((int)msgs[0]->index(), 0);
		}

		EXPECT_TRUE(r->progressMap_[2]->paused_);

		int j;
		// do a heartbeat
		for (j = 0; j < r->heartbeatTimeout_; ++j) {
			Message msg;
			msg.set_from(1);
			msg.set_to(1);
			msg.set_type(MsgBeat);
			r->step(msg);
		}
		EXPECT_TRUE(r->progressMap_[2]->paused_);

		// consume the heartbeat
		MessageVec tmp_msgs;
		r->readMessages(&tmp_msgs);
		EXPECT_EQ((int)tmp_msgs.size(), 1);
		EXPECT_EQ(tmp_msgs[0]->type(), MsgHeartbeat);

		// a heartbeat response will allow another message to be sent
		{
			Message msg;
			msg.set_from(2);
			msg.set_to(1);
			msg.set_type(MsgHeartbeatResp);
			r->step(msg);
		}
		r->readMessages(&msgs);
		EXPECT_EQ((int)msgs.size(), 1);
		EXPECT_EQ((int)msgs[0]->index(), 0);
		EXPECT_TRUE(r->progressMap_[2]->paused_);
	}
}

TEST(raftTests, TestSendAppendForProgressReplicate) {
	vector<uint64_t> peers;
	peers.push_back(1);
	peers.push_back(2);
	Storage *s = new MemoryStorage(&kDefaultLogger);
	raft *r = newTestRaft(1, peers, 10, 1, s);
	r->becomeCandidate();
	r->becomeLeader();

	MessageVec msgs;
	r->readMessages(&msgs);
	r->progressMap_[2]->becomeReplicate();

	size_t i;
	for (i = 0; i < 10; ++i) {
		EntryVec entries;
		Entry entry;
		entry.set_data("somedata");
		entries.push_back(entry);
		r->appendEntry(&entries);
		r->sendAppend(2);
		r->readMessages(&msgs);
		EXPECT_EQ((int)msgs.size(), 1);
	}
}

TEST(raftTests, TestSendAppendForProgressSnapshot) {
	vector<uint64_t> peers;
	peers.push_back(1);
	peers.push_back(2);
	Storage *s = new MemoryStorage(&kDefaultLogger);
	raft *r = newTestRaft(1, peers, 10, 1, s);
	r->becomeCandidate();
	r->becomeLeader();

	MessageVec msgs;
	r->readMessages(&msgs);
	r->progressMap_[2]->becomeSnapshot(10);

	size_t i;
	for (i = 0; i < 10; ++i) {
		EntryVec entries;
		Entry entry;
		entry.set_data("somedata");
		entries.push_back(entry);
		r->appendEntry(&entries);
		r->sendAppend(2);
		r->readMessages(&msgs);
		EXPECT_EQ((int)msgs.size(), 0);
	}
}

TEST(raftTests, TestRecvMsgUnreachable) {
  EntryVec prevEntries;
  {
    Entry entry;

    entry.set_term(1);
    entry.set_index(1);
    prevEntries.push_back(entry);

    entry.set_term(1);
    entry.set_index(2);
    prevEntries.push_back(entry);

    entry.set_term(1);
    entry.set_index(3);
    prevEntries.push_back(entry);
  }
	vector<uint64_t> peers;
	peers.push_back(1);
	peers.push_back(2);
	MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
	s->Append(prevEntries);
	raft *r = newTestRaft(1, peers, 10, 1, s);
	r->becomeCandidate();
	r->becomeLeader();

	// set node 2 to state replicate
	r->progressMap_[2]->match_ = 3;
	r->progressMap_[2]->becomeReplicate();
	r->progressMap_[2]->optimisticUpdate(5);

	{
		Message msg;
		msg.set_from(2);
		msg.set_to(1);
		msg.set_type(MsgUnreachable);
		r->step(msg);
	}

	EXPECT_EQ(r->progressMap_[2]->state_, ProgressStateProbe);
	EXPECT_EQ(r->progressMap_[2]->next_, r->progressMap_[2]->match_ + 1);
}

TEST(raftTests, TestRestore) {
  Snapshot ss;
  ss.mutable_metadata()->set_index(11);
  ss.mutable_metadata()->set_term(11);
  ss.mutable_metadata()->mutable_conf_state()->add_nodes(1);
  ss.mutable_metadata()->mutable_conf_state()->add_nodes(2);
  ss.mutable_metadata()->mutable_conf_state()->add_nodes(3);

  MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
  vector<uint64_t> peers;
  peers.push_back(1);
  peers.push_back(2);
  raft *r = newTestRaft(1, peers, 10, 1, s);

  EXPECT_TRUE(r->restore(ss));

  EXPECT_EQ(r->raftLog_->lastIndex(), ss.metadata().index());
  uint64_t term;
  r->raftLog_->term(ss.metadata().index(), &term);
  EXPECT_EQ(term, ss.metadata().term());

  EXPECT_FALSE(r->restore(ss));
}

TEST(raftTests, TestRestoreIgnoreSnapshot) {
  EntryVec prevEntries;
  {
    Entry entry;

    entry.set_term(1);
    entry.set_index(1);
    prevEntries.push_back(entry);

    entry.set_term(1);
    entry.set_index(2);
    prevEntries.push_back(entry);

    entry.set_term(1);
    entry.set_index(3);
    prevEntries.push_back(entry);
  }
	vector<uint64_t> peers;
	peers.push_back(1);
	peers.push_back(2);
	Storage *s = new MemoryStorage(&kDefaultLogger);
	raft *r = newTestRaft(1, peers, 10, 1, s);
	r->raftLog_->append(prevEntries);
  uint64_t commit = 1;
  r->raftLog_->commitTo(commit);

  Snapshot ss;
  ss.mutable_metadata()->set_index(commit);
  ss.mutable_metadata()->set_term(1);
  ss.mutable_metadata()->mutable_conf_state()->add_nodes(1);
  ss.mutable_metadata()->mutable_conf_state()->add_nodes(2);

  // ignore snapshot
  EXPECT_FALSE(r->restore(ss));

  EXPECT_EQ(r->raftLog_->committed_, commit);

  // ignore snapshot and fast forward commit
  ss.mutable_metadata()->set_index(commit + 1);
  EXPECT_FALSE(r->restore(ss));
  EXPECT_EQ(r->raftLog_->committed_, commit + 1);
}

TEST(raftTests, TestProvideSnap) {
  // restore the state machine from a snapshot so it has a compacted log and a snapshot
  Snapshot ss;
  ss.mutable_metadata()->set_index(11);
  ss.mutable_metadata()->set_term(11);
  ss.mutable_metadata()->mutable_conf_state()->add_nodes(1);
  ss.mutable_metadata()->mutable_conf_state()->add_nodes(2);

  MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
  vector<uint64_t> peers;
  peers.push_back(1);
  raft *r = newTestRaft(1, peers, 10, 1, s);

  r->restore(ss);

  r->becomeCandidate();
  r->becomeLeader();

  // force set the next of node 2, so that node 2 needs a snapshot
  r->progressMap_[2]->next_ = r->raftLog_->firstIndex();
  {
    Message msg;
    msg.set_from(2);
    msg.set_to(2);
    msg.set_type(MsgAppResp);
    msg.set_index(r->progressMap_[2]->next_ - 1);
    msg.set_reject(true);
    r->step(msg);
  }

  MessageVec msgs;
  r->readMessages(&msgs);
  EXPECT_EQ((int)msgs.size(), 1);
  EXPECT_EQ(msgs[0]->type(), MsgSnap);
}

TEST(raftTests, TestIgnoreProvidingSnap) {
  // restore the state machine from a snapshot so it has a compacted log and a snapshot
  Snapshot ss;
  ss.mutable_metadata()->set_index(11);
  ss.mutable_metadata()->set_term(11);
  ss.mutable_metadata()->mutable_conf_state()->add_nodes(1);
  ss.mutable_metadata()->mutable_conf_state()->add_nodes(2);

  MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
  vector<uint64_t> peers;
  peers.push_back(1);
  raft *r = newTestRaft(1, peers, 10, 1, s);

  r->restore(ss);

  r->becomeCandidate();
  r->becomeLeader();

	// force set the next of node 2, so that node 2 needs a snapshot
	// change node 2 to be inactive, expect node 1 ignore sending snapshot to 2
  r->progressMap_[2]->next_ = r->raftLog_->firstIndex() - 1;
  r->progressMap_[2]->recentActive_ = false;
  {
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgProp);
    msg.add_entries()->set_data("somedata");
    r->step(msg);
  }

  MessageVec msgs;
  r->readMessages(&msgs);
  EXPECT_EQ((int)msgs.size(), 0);
}

TEST(raftTests, TestRestoreFromSnapMsg) {
}

TEST(raftTests, TestSlowNodeRestore) {
  vector<stateMachine*> peers;
  peers.push_back(NULL);
  peers.push_back(NULL);
  peers.push_back(NULL);

  network *net = newNetwork(peers);
  {
    vector<Message> msgs;
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgHup);
    msgs.push_back(msg);
    net->send(&msgs);
  }

  net->isolate(3);
  size_t i;
  for (i = 0; i <= 100; ++i) {
      vector<Message> msgs;
      Message msg;
      msg.set_from(1);
      msg.set_to(1);
      msg.set_type(MsgProp);
      msg.add_entries();
      msgs.push_back(msg);
    net->send(&msgs);
  }

  raft *leader = (raft*)net->peers[1]->data();
  EntryVec entries;
  nextEnts(leader, net->storage[1], &entries);
  ConfState cs;
  vector<uint64_t> nodes;
  leader->nodes(&nodes);
  size_t j;
  for (j = 0; j < nodes.size(); ++j) {
    cs.add_nodes(nodes[j]);
  }
  net->storage[1]->CreateSnapshot(leader->raftLog_->applied_, &cs, "", NULL);
  net->storage[1]->Compact(leader->raftLog_->applied_);

  net->recover();
	// send heartbeats so that the leader can learn everyone is active.
	// node 3 will only be considered as active when node 1 receives a reply from it.
  do {
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgBeat);
    vector<Message> msgs;
    msgs.push_back(msg);
    net->send(&msgs);
  } while (leader->progressMap_[3]->recentActive_ == false);

  // trigger a snapshot
  {
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgProp);
    msg.add_entries();
    vector<Message> msgs;
    net->send(&msgs);
  }  

  raft *follower = (raft*)net->peers[3]->data();

  // trigger a commit
  {
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgProp);
    msg.add_entries();
    vector<Message> msgs;
    net->send(&msgs);
  }  
  EXPECT_EQ(follower->raftLog_->committed_, leader->raftLog_->committed_);
}

// TestStepConfig tests that when raft step msgProp in EntryConfChange type,
// it appends the entry to log and sets pendingConf to be true.
TEST(raftTests, TestStepConfig) {
  // a raft that cannot make progress
  MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
  vector<uint64_t> peers;
  peers.push_back(1);
  peers.push_back(2);
  raft *r = newTestRaft(1, peers, 10, 1, s);

  r->becomeCandidate();
  r->becomeLeader();

  uint64_t index = r->raftLog_->lastIndex();
  Message msg;
  msg.set_from(1);
  msg.set_to(1);
  msg.set_type(MsgProp);
  msg.add_entries()->set_type(EntryConfChange);
  r->step(msg);

  EXPECT_EQ(r->raftLog_->lastIndex(), index + 1);
  EXPECT_TRUE(r->pendingConf_);
}

// TestStepIgnoreConfig tests that if raft step the second msgProp in
// EntryConfChange type when the first one is uncommitted, the node will set
// the proposal to noop and keep its original state.
TEST(raftTests, TestStepIgnoreConfig) {
  // a raft that cannot make progress
  MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
  vector<uint64_t> peers;
  peers.push_back(1);
  peers.push_back(2);
  raft *r = newTestRaft(1, peers, 10, 1, s);

  r->becomeCandidate();
  r->becomeLeader();

  {
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgProp);
    msg.add_entries()->set_type(EntryConfChange);
    r->step(msg);
  }
  uint64_t index = r->raftLog_->lastIndex();
  bool pendingConf = r->pendingConf_;

  {
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgProp);
    msg.add_entries()->set_type(EntryConfChange);
    r->step(msg);
  }
  EntryVec wents, ents;
  {
    Entry entry;
    entry.set_type(EntryNormal);
    entry.set_term(1);
    entry.set_index(3);
    wents.push_back(entry);
  }
  int err = r->raftLog_->entries(index + 1, kNoLimit, &ents);

  EXPECT_EQ(err, OK);
  EXPECT_TRUE(isDeepEqualEntries(wents, ents));
  EXPECT_EQ(r->pendingConf_, pendingConf);
}

// TestRecoverPendingConfig tests that new leader recovers its pendingConf flag
// based on uncommitted entries.
TEST(raftTests, TestRecoverPendingConfig) {
  struct tmp {
    EntryType type;
    bool pending;
  
    tmp(EntryType t, bool pending)
      : type(t), pending(pending) {}
  };

  vector<tmp> tests;
  tests.push_back(tmp(EntryNormal, false));
  tests.push_back(tmp(EntryConfChange, true));
  size_t i;
  for (i = 0; i < tests.size(); ++i) {
    tmp &t = tests[i];
    MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
    vector<uint64_t> peers;
    peers.push_back(1);
    peers.push_back(2);
    raft *r = newTestRaft(1, peers, 10, 1, s);

    EntryVec entries;
    Entry entry;
    entry.set_type(t.type);
    entries.push_back(entry);
    r->appendEntry(&entries);
    r->becomeCandidate();
    r->becomeLeader();
    EXPECT_EQ(t.pending, r->pendingConf_);
  }
}

TEST(raftTests, TestRecoverDoublePendingConfig) {
}

// TestAddNode tests that addNode could update pendingConf and nodes correctly.
TEST(raftTests, TestAddNode) {
  MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
  vector<uint64_t> peers;
  peers.push_back(1);
  raft *r = newTestRaft(1, peers, 10, 1, s);

  r->addNode(2);
  EXPECT_FALSE(r->pendingConf_);
  vector<uint64_t> nodes, wnodes;
  r->nodes(&nodes);

  wnodes.push_back(1);
  wnodes.push_back(2);
  EXPECT_TRUE(isDeepEqualNodes(nodes, wnodes));
}

// TestRemoveNode tests that removeNode could update pendingConf, nodes and
// and removed list correctly.
TEST(raftTests, TestRemoveNode) {
  MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
  vector<uint64_t> peers;
  peers.push_back(1);
  peers.push_back(2);
  raft *r = newTestRaft(1, peers, 10, 1, s);

  r->removeNode(2);
  EXPECT_FALSE(r->pendingConf_);
  vector<uint64_t> nodes, wnodes;
  r->nodes(&nodes);

  wnodes.push_back(1);
  EXPECT_TRUE(isDeepEqualNodes(nodes, wnodes));

  r->removeNode(1);
  wnodes.clear();
  r->nodes(&nodes);
  EXPECT_TRUE(isDeepEqualNodes(nodes, wnodes));
}

TEST(raftTests, TestPromotable) {
  struct tmp {
    vector<uint64_t> peers;
    bool wp;

    tmp(vector<uint64_t> p, bool wp)
      : peers(p), wp(wp) {}
  };
  vector<tmp> tests;
  {
    vector<uint64_t> peers;
    peers.push_back(1);
    tests.push_back(tmp(peers, true));
  }
  {
    vector<uint64_t> peers;
    peers.push_back(1);
    peers.push_back(2);
    peers.push_back(3);
    tests.push_back(tmp(peers, true));
  }
  {
    vector<uint64_t> peers;
    tests.push_back(tmp(peers, false));
  }
  {
    vector<uint64_t> peers;
    peers.push_back(2);
    peers.push_back(3);
    tests.push_back(tmp(peers, false));
  }
  size_t i;
  for (i = 0; i < tests.size(); ++i) {
    tmp &t = tests[i];

    MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
    raft *r = newTestRaft(1, t.peers, 5, 1, s);
    EXPECT_EQ(r->promotable(), t.wp);
  }
}

TEST(raftTests, TestRaftNodes) {
  struct tmp {
    vector<uint64_t> ids, wids;

    tmp(vector<uint64_t> p, vector<uint64_t> wp)
      : ids(p), wids(wp) {}
  };
  vector<tmp> tests;
  {
    vector<uint64_t> peers;
    peers.push_back(1);
    peers.push_back(2);
    peers.push_back(3);
    tests.push_back(tmp(peers, peers));
  }
  {
    vector<uint64_t> peers, wps;
    peers.push_back(3);
    peers.push_back(2);
    peers.push_back(1);
    wps.push_back(1);
    wps.push_back(2);
    wps.push_back(3);
    tests.push_back(tmp(peers, wps));
  }
  size_t i;
  for (i = 0; i < tests.size(); ++i) {
    tmp &t = tests[i];

    MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
    raft *r = newTestRaft(1, t.ids, 10, 1, s);
    vector<uint64_t> nodes;
    r->nodes(&nodes);
    EXPECT_TRUE(isDeepEqualNodes(nodes, t.wids));
  }
}

void testCampaignWhileLeader(bool prevote) {
  vector<uint64_t> peers;
  Storage *s = new MemoryStorage(&kDefaultLogger);
  peers.push_back(1);
  Config *c = newTestConfig(1, peers, 5, 1, s);
  c->preVote = prevote;
  
  raft *r = newRaft(c);
  EXPECT_EQ(r->state_, StateFollower);

	// We don't call campaign() directly because it comes after the check
	// for our current state.
  {
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgHup);
    r->step(msg);
  }
  EXPECT_EQ(r->state_, StateLeader);

  uint64_t term = r->term_;
  {
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgHup);
    r->step(msg);
  }
  EXPECT_EQ(r->state_, StateLeader);
  EXPECT_EQ(r->term_, term);
}

TEST(raftTests, TestCampaignWhileLeader) {
  testCampaignWhileLeader(false);
}

TEST(raftTests, TestPreCampaignWhileLeader) {
  testCampaignWhileLeader(true);
}

// TestCommitAfterRemoveNode verifies that pending commands can become
// committed when a config change reduces the quorum requirements.
TEST(raftTests, TestCommitAfterRemoveNode) {
  // Create a cluster with two nodes.
  MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
  vector<uint64_t> peers;
  peers.push_back(1);
  peers.push_back(2);
  raft *r = newTestRaft(1, peers, 5, 1, s);

  r->becomeCandidate();
  r->becomeLeader();

  // Begin to remove the second node.
  ConfChange cc;
  cc.set_type(ConfChangeRemoveNode);
  cc.set_nodeid(2);
  string ccdata;
  cc.SerializeToString(&ccdata);

  {
    Message msg;
    msg.set_type(MsgProp);
    Entry *ent = msg.add_entries();
    ent->set_type(EntryConfChange);
    ent->set_data(ccdata);
    r->step(msg);
  }

  EntryVec entries;
  nextEnts(r, s, &entries);
  EXPECT_EQ((int)entries.size(), 0);

  uint64_t ccIndex = r->raftLog_->lastIndex();

  // While the config change is pending, make another proposal.
  {
    Message msg;
    msg.set_type(MsgProp);
    Entry *ent = msg.add_entries();
    ent->set_type(EntryNormal);
    ent->set_data("hello");
    r->step(msg);
  }

  // Node 2 acknowledges the config change, committing it.
  {
    Message msg;
    msg.set_type(MsgAppResp);
    msg.set_from(2);
    msg.set_index(ccIndex);
    r->step(msg);
  }

  nextEnts(r, s, &entries);
  EXPECT_EQ((int)entries.size(), 2);
  EXPECT_FALSE(entries[0].type() != EntryNormal || entries[0].has_data());
  EXPECT_FALSE(entries[1].type() != EntryConfChange);

	// Apply the config change. This reduces quorum requirements so the
	// pending command can now commit.
  r->removeNode(2);
  nextEnts(r, s, &entries);
  EXPECT_FALSE(entries.size() != 1 || entries[0].type() != EntryNormal || entries[0].data() != "hello");
}

void checkLeaderTransferState(raft *r, StateType state, uint64_t leader) {
  EXPECT_FALSE(r->state_ != state || r->leader_ != leader);
  EXPECT_EQ(r->leadTransferee_, kEmptyPeerId);
}

// TestLeaderTransferToUpToDateNode verifies transferring should succeed
// if the transferee has the most up-to-date log entries when transfer starts.
TEST(raftTests, TestLeaderTransferToUpToDateNode) {
  vector<stateMachine*> peers;
  peers.push_back(NULL);
  peers.push_back(NULL);
  peers.push_back(NULL);

  network *net = newNetwork(peers);
  {
    Message msg;
    vector<Message> msgs;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgHup);
    msgs.push_back(msg);
    net->send(&msgs);
  }

  raft *leader = (raft*)net->peers[1]->data();
  EXPECT_EQ((int)leader->leader_, 1);

  // Transfer leadership to 2.
  {
    Message msg;
    vector<Message> msgs;
    msg.set_from(2);
    msg.set_to(1);
    msg.set_type(MsgTransferLeader);
    msgs.push_back(msg);
    net->send(&msgs);
  }

  checkLeaderTransferState(leader, StateFollower, 2);
  // After some log replication, transfer leadership back to 1.
  {
    Message msg;
    vector<Message> msgs;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgProp);
    msg.add_entries();
    msgs.push_back(msg);
    net->send(&msgs);
  }
  {
    Message msg;
    vector<Message> msgs;
    msg.set_from(1);
    msg.set_to(2);
    msg.set_type(MsgTransferLeader);
    msgs.push_back(msg);
    net->send(&msgs);
  }
  checkLeaderTransferState(leader, StateLeader, 1);
}

// TestLeaderTransferToUpToDateNodeFromFollower verifies transferring should succeed
// if the transferee has the most up-to-date log entries when transfer starts.
// Not like TestLeaderTransferToUpToDateNode, where the leader transfer message
// is sent to the leader, in this test case every leader transfer message is sent
// to the follower.
TEST(raftTests, TestLeaderTransferToUpToDateNodeFromFollower) {
  vector<stateMachine*> peers;
  peers.push_back(NULL);
  peers.push_back(NULL);
  peers.push_back(NULL);

  network *net = newNetwork(peers);
  {
    Message msg;
    vector<Message> msgs;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgHup);
    msgs.push_back(msg);
    net->send(&msgs);
  }

  raft *leader = (raft*)net->peers[1]->data();
  EXPECT_EQ((int)leader->leader_, 1);

  // Transfer leadership to 2.
  {
    Message msg;
    vector<Message> msgs;
    msg.set_from(2);
    msg.set_to(1);
    msg.set_type(MsgTransferLeader);
    msgs.push_back(msg);
    net->send(&msgs);
  }

  checkLeaderTransferState(leader, StateFollower, 2);
  // After some log replication, transfer leadership back to 1.
  {
    Message msg;
    vector<Message> msgs;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgProp);
    msg.add_entries();
    msgs.push_back(msg);
    net->send(&msgs);
  }
  {
    Message msg;
    vector<Message> msgs;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgTransferLeader);
    msgs.push_back(msg);
    net->send(&msgs);
  }
  checkLeaderTransferState(leader, StateLeader, 1);
}

// TestLeaderTransferWithCheckQuorum ensures transferring leader still works
// even the current leader is still under its leader lease
TEST(raftTests, TestLeaderTransferWithCheckQuorum) {
  vector<stateMachine*> peers;
  peers.push_back(NULL);
  peers.push_back(NULL);
  peers.push_back(NULL);

  network *net = newNetwork(peers);
  int i;
  for (i = 1; i < 4; ++i) {
    raft *r = (raft*)net->peers[i]->data();
    r->checkQuorum_ = true;
    r->randomizedElectionTimeout_ = r->electionTimeout_ + i;
  }
  
  raft *r = (raft*)net->peers[2]->data();
  for (i = 0; i < r->electionTimeout_; ++i) {
    r->tick();
  }

  {
    Message msg;
    vector<Message> msgs;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgHup);
    msgs.push_back(msg);
    net->send(&msgs);
  }

  raft *leader = (raft*)net->peers[1]->data();
  EXPECT_EQ((int)leader->leader_, 1);

  // Transfer leadership to 2.
  {
    Message msg;
    vector<Message> msgs;
    msg.set_from(2);
    msg.set_to(1);
    msg.set_type(MsgTransferLeader);
    msgs.push_back(msg);
    net->send(&msgs);
  }

  checkLeaderTransferState(leader, StateFollower, 2);
  // After some log replication, transfer leadership back to 1.
  {
    Message msg;
    vector<Message> msgs;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgProp);
    msg.add_entries();
    msgs.push_back(msg);
    net->send(&msgs);
  }
  {
    Message msg;
    vector<Message> msgs;
    msg.set_from(1);
    msg.set_to(2);
    msg.set_type(MsgTransferLeader);
    msgs.push_back(msg);
    net->send(&msgs);
  }
  checkLeaderTransferState(leader, StateLeader, 1);
}

TEST(raftTests, TestLeaderTransferToSlowFollower) {
  vector<stateMachine*> peers;
  peers.push_back(NULL);
  peers.push_back(NULL);
  peers.push_back(NULL);

  network *net = newNetwork(peers);

  {
    Message msg;
    vector<Message> msgs;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgHup);
    msgs.push_back(msg);
    net->send(&msgs);
  }

  net->isolate(3);

  {
    Message msg;
    vector<Message> msgs;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgProp);
    msg.add_entries();
    msgs.push_back(msg);
    net->send(&msgs);
  }

  net->recover();
  raft *leader = (raft*)net->peers[1]->data();
  EXPECT_EQ((int)leader->progressMap_[3]->match_, 1);

  // Transfer leadership to 3 when node 3 is lack of log.
  {
    Message msg;
    vector<Message> msgs;
    msg.set_from(3);
    msg.set_to(1);
    msg.set_type(MsgTransferLeader);
    msgs.push_back(msg);
    net->send(&msgs);
  }
  checkLeaderTransferState(leader, StateFollower, 3);
}

TEST(raftTests, TestLeaderTransferAfterSnapshot) {
  vector<stateMachine*> peers;
  peers.push_back(NULL);
  peers.push_back(NULL);
  peers.push_back(NULL);

  network *net = newNetwork(peers);

  {
    Message msg;
    vector<Message> msgs;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgHup);
    msgs.push_back(msg);
    net->send(&msgs);
  }

  net->isolate(3);

  {
    Message msg;
    vector<Message> msgs;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgProp);
    msg.add_entries();
    msgs.push_back(msg);
    net->send(&msgs);
  }

  raft *leader = (raft*)net->peers[1]->data();
  EntryVec entries;
  nextEnts(leader, net->storage[1], &entries);
  ConfState cs;
  vector<uint64_t> nodes;
  leader->nodes(&nodes);
  size_t j;
  for (j = 0; j < nodes.size(); ++j) {
    cs.add_nodes(nodes[j]);
  }
  net->storage[1]->CreateSnapshot(leader->raftLog_->applied_, &cs, "", NULL);
  net->storage[1]->Compact(leader->raftLog_->applied_);

  net->recover();
  EXPECT_EQ((int)leader->progressMap_[3]->match_, 1);

  // Transfer leadership to 3 when node 3 is lack of log.
  {
    Message msg;
    vector<Message> msgs;
    msg.set_from(3);
    msg.set_to(1);
    msg.set_type(MsgTransferLeader);
    msgs.push_back(msg);
    net->send(&msgs);
  }
  // Send pb.MsgHeartbeatResp to leader to trigger a snapshot for node 3.
  {
    Message msg;
    vector<Message> msgs;
    msg.set_from(3);
    msg.set_to(1);
    msg.set_type(MsgHeartbeatResp);
    msgs.push_back(msg);
    net->send(&msgs);
  }
  checkLeaderTransferState(leader, StateFollower, 3);
}

TEST(raftTests, TestLeaderTransferToSelf) {
  vector<stateMachine*> peers;
  peers.push_back(NULL);
  peers.push_back(NULL);
  peers.push_back(NULL);

  network *net = newNetwork(peers);

  {
    Message msg;
    vector<Message> msgs;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgHup);
    msgs.push_back(msg);
    net->send(&msgs);
  }

  raft *leader = (raft*)net->peers[1]->data();

  // Transfer leadership to self, there will be noop.
  {
    Message msg;
    vector<Message> msgs;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgTransferLeader);
    msgs.push_back(msg);
    net->send(&msgs);
  }
  checkLeaderTransferState(leader, StateLeader, 1);
}

TEST(raftTests, TestLeaderTransferToNonExistingNode) {
  vector<stateMachine*> peers;
  peers.push_back(NULL);
  peers.push_back(NULL);
  peers.push_back(NULL);

  network *net = newNetwork(peers);

  {
    Message msg;
    vector<Message> msgs;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgHup);
    msgs.push_back(msg);
    net->send(&msgs);
  }

  raft *leader = (raft*)net->peers[1]->data();

  // Transfer leadership to non-existing node, there will be noop.
  {
    Message msg;
    vector<Message> msgs;
    msg.set_from(4);
    msg.set_to(1);
    msg.set_type(MsgTransferLeader);
    msgs.push_back(msg);
    net->send(&msgs);
  }
  checkLeaderTransferState(leader, StateLeader, 1);
}

TEST(raftTests, TestLeaderTransferTimeout) {
  vector<stateMachine*> peers;
  peers.push_back(NULL);
  peers.push_back(NULL);
  peers.push_back(NULL);

  network *net = newNetwork(peers);

  {
    Message msg;
    vector<Message> msgs;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgHup);
    msgs.push_back(msg);
    net->send(&msgs);
  }

  net->isolate(3);
  raft *leader = (raft*)net->peers[1]->data();

  // Transfer leadership to isolated node, wait for timeout.
  {
    Message msg;
    vector<Message> msgs;
    msg.set_from(3);
    msg.set_to(1);
    msg.set_type(MsgTransferLeader);
    msgs.push_back(msg);
    net->send(&msgs);
  }

  EXPECT_EQ((int)leader->leadTransferee_, 3);
  int i;
  for (i = 0; i < leader->heartbeatTimeout_; ++i) {
    leader->tick();
  }
  EXPECT_EQ((int)leader->leadTransferee_, 3);
  for (i = 0; i < leader->electionTimeout_ - leader->heartbeatTimeout_; ++i) {
    leader->tick();
  }
  checkLeaderTransferState(leader, StateLeader, 1);
}

TEST(raftTests, TestLeaderTransferIgnoreProposal) {
  vector<stateMachine*> peers;
  peers.push_back(NULL);
  peers.push_back(NULL);
  peers.push_back(NULL);

  network *net = newNetwork(peers);

  {
    Message msg;
    vector<Message> msgs;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgHup);
    msgs.push_back(msg);
    net->send(&msgs);
  }

  net->isolate(3);
  raft *leader = (raft*)net->peers[1]->data();

  // Transfer leadership to isolated node to let transfer pending, then send proposal.
  {
    Message msg;
    vector<Message> msgs;
    msg.set_from(3);
    msg.set_to(1);
    msg.set_type(MsgTransferLeader);
    msgs.push_back(msg);
    net->send(&msgs);
  }
  EXPECT_EQ((int)leader->leadTransferee_, 3);

  {
    Message msg;
    vector<Message> msgs;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgProp);
    msg.add_entries();
    msgs.push_back(msg);
    net->send(&msgs);
  }
  EXPECT_EQ((int)leader->progressMap_[1]->match_, 1);
}

TEST(raftTests, TestLeaderTransferReceiveHigherTermVote) {
  vector<stateMachine*> peers;
  peers.push_back(NULL);
  peers.push_back(NULL);
  peers.push_back(NULL);

  network *net = newNetwork(peers);

  {
    Message msg;
    vector<Message> msgs;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgHup);
    msgs.push_back(msg);
    net->send(&msgs);
  }

  net->isolate(3);
  raft *leader = (raft*)net->peers[1]->data();

  // Transfer leadership to isolated node to let transfer pending.
  {
    Message msg;
    vector<Message> msgs;
    msg.set_from(3);
    msg.set_to(1);
    msg.set_type(MsgTransferLeader);
    msgs.push_back(msg);
    net->send(&msgs);
  }
  EXPECT_EQ((int)leader->leadTransferee_, 3);

  {
    Message msg;
    vector<Message> msgs;
    msg.set_from(2);
    msg.set_to(2);
    msg.set_type(MsgHup);
    msg.set_index(1);
    msg.set_term(2);
    msgs.push_back(msg);
    net->send(&msgs);
  }
  checkLeaderTransferState(leader, StateFollower, 2);
}

TEST(raftTests, TestLeaderTransferRemoveNode) {
  vector<stateMachine*> peers;
  peers.push_back(NULL);
  peers.push_back(NULL);
  peers.push_back(NULL);

  network *net = newNetwork(peers);

  {
    Message msg;
    vector<Message> msgs;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgHup);
    msgs.push_back(msg);
    net->send(&msgs);
  }

  net->ignore(MsgTimeoutNow);
  raft *leader = (raft*)net->peers[1]->data();

  // The leadTransferee is removed when leadship transferring.
  {
    Message msg;
    vector<Message> msgs;
    msg.set_from(3);
    msg.set_to(1);
    msg.set_type(MsgTransferLeader);
    msgs.push_back(msg);
    net->send(&msgs);
  }
  EXPECT_EQ((int)leader->leadTransferee_, 3);

  leader->removeNode(3);
  checkLeaderTransferState(leader, StateLeader, 1);
}

// TestLeaderTransferBack verifies leadership can transfer back to self when last transfer is pending.
TEST(raftTests, TestLeaderTransferBack) {
  vector<stateMachine*> peers;
  peers.push_back(NULL);
  peers.push_back(NULL);
  peers.push_back(NULL);

  network *net = newNetwork(peers);

  {
    Message msg;
    vector<Message> msgs;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgHup);
    msgs.push_back(msg);
    net->send(&msgs);
  }

  net->isolate(3);
  raft *leader = (raft*)net->peers[1]->data();

  // The leadTransferee is removed when leadship transferring.
  {
    Message msg;
    vector<Message> msgs;
    msg.set_from(3);
    msg.set_to(1);
    msg.set_type(MsgTransferLeader);
    msgs.push_back(msg);
    net->send(&msgs);
  }
  EXPECT_EQ((int)leader->leadTransferee_, 3);

  // Transfer leadership back to self.
  {
    Message msg;
    vector<Message> msgs;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgTransferLeader);
    msgs.push_back(msg);
    net->send(&msgs);
  }

  checkLeaderTransferState(leader, StateLeader, 1);
}

// TestLeaderTransferSecondTransferToAnotherNode verifies leader can transfer to another node
// when last transfer is pending.
TEST(raftTests, TestLeaderTransferSecondTransferToAnotherNode) {
  vector<stateMachine*> peers;
  peers.push_back(NULL);
  peers.push_back(NULL);
  peers.push_back(NULL);

  network *net = newNetwork(peers);

  {
    Message msg;
    vector<Message> msgs;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgHup);
    msgs.push_back(msg);
    net->send(&msgs);
  }

  net->isolate(3);
  raft *leader = (raft*)net->peers[1]->data();

  {
    Message msg;
    vector<Message> msgs;
    msg.set_from(3);
    msg.set_to(1);
    msg.set_type(MsgTransferLeader);
    msgs.push_back(msg);
    net->send(&msgs);
  }
  EXPECT_EQ((int)leader->leadTransferee_, 3);

  // Transfer leadership to another node.
  {
    Message msg;
    vector<Message> msgs;
    msg.set_from(2);
    msg.set_to(1);
    msg.set_type(MsgTransferLeader);
    msgs.push_back(msg);
    net->send(&msgs);
  }

  checkLeaderTransferState(leader, StateFollower, 2);
}

// TestLeaderTransferSecondTransferToSameNode verifies second transfer leader request
// to the same node should not extend the timeout while the first one is pending.
TEST(raftTests, TestLeaderTransferSecondTransferToSameNode) {
  vector<stateMachine*> peers;
  peers.push_back(NULL);
  peers.push_back(NULL);
  peers.push_back(NULL);

  network *net = newNetwork(peers);

  {
    Message msg;
    vector<Message> msgs;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgHup);
    msgs.push_back(msg);
    net->send(&msgs);
  }

  net->isolate(3);
  raft *leader = (raft*)net->peers[1]->data();

  {
    Message msg;
    vector<Message> msgs;
    msg.set_from(3);
    msg.set_to(1);
    msg.set_type(MsgTransferLeader);
    msgs.push_back(msg);
    net->send(&msgs);
  }
  EXPECT_EQ((int)leader->leadTransferee_, 3);

  int i;
  for (i = 0; i < leader->heartbeatTimeout_; ++i) {
    leader->tick();
  }

  // Second transfer leadership request to the same node.
  {
    Message msg;
    vector<Message> msgs;
    msg.set_from(3);
    msg.set_to(1);
    msg.set_type(MsgTransferLeader);
    msgs.push_back(msg);
    net->send(&msgs);
  }
  for (i = 0; i < leader->electionTimeout_ - leader->heartbeatTimeout_; ++i) {
    leader->tick();
  }
  checkLeaderTransferState(leader, StateLeader, 1);
}

// TestTransferNonMember verifies that when a MsgTimeoutNow arrives at
// a node that has been removed from the group, nothing happens.
// (previously, if the node also got votes, it would panic as it
// transitioned to StateLeader)
TEST(raftTests, TestTransferNonMember) {
  vector<uint64_t> peers;
  peers.push_back(2);
  peers.push_back(3);
  peers.push_back(4);
  Storage *s = new MemoryStorage(&kDefaultLogger);
  raft *r = newTestRaft(1, peers, 5, 1, s);

  {
    Message msg;
    msg.set_from(2);
    msg.set_to(1);
    msg.set_type(MsgTimeoutNow);
    r->step(msg);
  }

  {
    Message msg;
    msg.set_from(2);
    msg.set_to(1);
    msg.set_type(MsgVoteResp);
    r->step(msg);
  }
  {
    Message msg;
    msg.set_from(3);
    msg.set_to(1);
    msg.set_type(MsgVoteResp);
    r->step(msg);
  }

  EXPECT_EQ(r->state_, StateFollower);
}
