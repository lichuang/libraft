#include <gtest/gtest.h>
#include "libraft.h"
#include "util.h"
#include "raft.h"
#include "memory_storage.h"
#include "default_logger.h"
#include "progress.h"
#include "raft_test_util.h"

stateMachine *nopStepper = new blackHole();

void preVoteConfig(Config *c) {
  c->preVote = true; 
}

raftStateMachine* entsWithConfig(ConfigFun fun, const vector<uint64_t>& terms) {
  Storage *s = new MemoryStorage(&kDefaultLogger);

  vector<Entry> entries;
  int i;
  for (i = 0; i < terms.size(); ++i) {
    Entry entry;
    entry.set_index(i + 1);
    entry.set_term(terms[i]);
    entries.push_back(entry);
  }
  s->Append(&entries);

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
  Storage *s = new MemoryStorage(&kDefaultLogger);
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

  int i;
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
  EXPECT_EQ(p.match_, 1);
  EXPECT_EQ(p.next_, p.match_ + 1);
}

TEST(raftTests, TestProgressBecomeSnapshot) {
  Progress p(5, 256, &kDefaultLogger);
  p.state_ = ProgressStateProbe;
  p.match_ = 1;

  p.becomeSnapshot(10);
  EXPECT_EQ(p.state_, ProgressStateSnapshot);
  EXPECT_EQ(p.match_, 1);
  EXPECT_EQ(p.pendingSnapshot_, 10);
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

  int i;
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
  int i;
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
  int i;
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
  r->prs_[2]->paused_ = true;

  {
    Message *msg = new Message();
    msg->set_from(1);
    msg->set_to(1);
    msg->set_type(MsgBeat);

    r->step(msg);
    EXPECT_TRUE(r->prs_[2]->paused_);
  }

  r->prs_[2]->becomeReplicate();
  {
    Message *msg = new Message();
    msg->set_from(2);
    msg->set_to(1);
    msg->set_type(MsgHeartbeatResp);

    r->step(msg);
    EXPECT_FALSE(r->prs_[2]->paused_);
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
    Message *msg = new Message();
    msg->set_from(1);
    msg->set_to(1);
    msg->set_type(MsgProp);
    Entry *entry = msg->add_entries();
    entry->set_data("somedata");
    r->step(msg);
  }
  {
    Message *msg = new Message();
    msg->set_from(1);
    msg->set_to(1);
    msg->set_type(MsgProp);
    Entry *entry = msg->add_entries();
    entry->set_data("somedata");
    r->step(msg);
  }
  {
    Message *msg = new Message();
    msg->set_from(1);
    msg->set_to(1);
    msg->set_type(MsgProp);
    Entry *entry = msg->add_entries();
    entry->set_data("somedata");
    r->step(msg);
  }

  vector<Message *> msgs;
  r->readMessages(&msgs);
  EXPECT_EQ(msgs.size(), 1);

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
  int i;
  for (i = 0; i < tests.size(); ++i) {
    tmp& t = tests[i];
    Message *msg = new Message(); 
    msg->set_from(1);
    msg->set_to(1);
    msg->set_type(MsgHup);
    vector<Message*> msgs;
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
  int i;
  for (i = 1; i <= 3; i++) {
    Message *msg = new Message();
    msg->set_from(i);
    msg->set_to(i);
    msg->set_type(MsgHup);
    vector<Message*> msgs;
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
    Message *msg = new Message();
    msg->set_from(1);
    msg->set_to(1);
    msg->set_type(MsgHup);
    vector<Message*> msgs;
    msgs.push_back(msg);
    net->send(&msgs);
  }
  
  raft *r1 = (raft*)net->peers[1]->data();
  EXPECT_EQ(r1->state_, StateFollower);
  EXPECT_EQ(r1->term_, 2);

  // Node 1 campaigns again with a higher term. This time it succeeds.
  {
    Message *msg = new Message();
    msg->set_from(1);
    msg->set_to(1);
    msg->set_type(MsgHup);
    vector<Message*> msgs;
    msgs.push_back(msg);
    net->send(&msgs);
  }
  EXPECT_EQ(r1->state_, StateLeader);
  EXPECT_EQ(r1->term_, 3);

  // Now all nodes agree on a log entry with term 1 at index 1 (and
  // term 3 at index 2).
  map<uint64_t, stateMachine*>::iterator iter;
  for (iter = net->peers.begin(); iter != net->peers.end(); ++iter) {
    raft *r = (raft*)iter->second->data();
    EntryVec entries;
    r->raftLog_->allEntries(&entries);
    EXPECT_EQ(entries.size(), 2);
    EXPECT_EQ(entries[0].term(), 1);
    EXPECT_EQ(entries[1].term(), 3);
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

  int i;
  for (i = 0; i < (int)numStates; ++i) {
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
    }

    // Note that setting our state above may have advanced r.Term
    // past its initial value.
    uint64_t origTerm = r->term_;
    uint64_t newTerm  = r->term_ + 1;

    Message *msg = new Message();
    msg->set_from(2);
    msg->set_to(1);
    msg->set_type(vt);
    msg->set_term(newTerm);
    msg->set_logterm(newTerm);
    msg->set_index(42);
    int err = r->step(msg);

    EXPECT_EQ(err, OK);
    EXPECT_EQ(r->msgs_.size(), 1);
    Message *resp = r->msgs_[0];
    EXPECT_EQ(resp->type(), voteRespMsgType(vt));
    EXPECT_FALSE(resp->reject());

    if (vt == MsgVote) {
      // If this was a real vote, we reset our state and term.
      EXPECT_EQ(r->state_, StateFollower);
      EXPECT_EQ(r->term_, newTerm);
      EXPECT_EQ(r->vote_, 2);
    } else {
      // In a prevote, nothing changes.
      EXPECT_EQ(r->state_, st);
      EXPECT_EQ(r->term_, origTerm);
      // if st == StateFollower or StatePreCandidate, r hasn't voted yet.
      // In StateCandidate or StateLeader, it's voted for itself.
      EXPECT_FALSE(r->vote_ != None && r->vote_ != 1);
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
    vector<Message*> msgs;
    uint64_t wcommitted;

    tmp(network *net, vector<Message*> msgs, uint64_t w)
      : net(net), msgs(msgs), wcommitted(w) {
    }
  };

  vector<tmp> tests;
  {
    vector<stateMachine*> peers;
    peers.push_back(NULL);
    peers.push_back(NULL);
    peers.push_back(NULL);

    vector<Message*> msgs;
    {
      Message *msg = new Message();
      msg->set_from(1);
      msg->set_to(1);
      msg->set_type(MsgProp);
      Entry *entry = msg->add_entries();
      entry->set_data("somedata");

      msgs.push_back(msg);
    }
    //tests.push_back(tmp(newNetwork(peers), msgs, 2));
  }
  {
    vector<stateMachine*> peers;
    peers.push_back(NULL);
    peers.push_back(NULL);
    peers.push_back(NULL);

    vector<Message*> msgs;
    {
      Message *msg = new Message();
      msg->set_from(1);
      msg->set_to(1);
      msg->set_type(MsgProp);
      Entry *entry = msg->add_entries();
      entry->set_data("somedata");

      msgs.push_back(msg);
    }
    {
      Message *msg = new Message();
      msg->set_from(1);
      msg->set_to(2);
      msg->set_type(MsgHup);

      msgs.push_back(msg);
    }
    {
      Message *msg = new Message();
      msg->set_from(1);
      msg->set_to(2);
      msg->set_type(MsgProp);

      Entry *entry = msg->add_entries();
      entry->set_data("somedata");
      msgs.push_back(msg);
    }
    tests.push_back(tmp(newNetwork(peers), msgs, 4));
  }
  int i;
  for (i = 0; i < tests.size(); ++i) {
    tmp &t = tests[i];
    Message *msg = new Message();
    msg->set_from(1);
    msg->set_to(1);
    msg->set_type(MsgHup);
    vector<Message*> msgs;
    msgs.push_back(msg);
    t.net->send(&msgs);

    int j;
    for (j = 0; j < t.msgs.size(); ++j) {
      vector<Message*> msgs;
      msgs.push_back(t.msgs[j]);
      t.net->send(&msgs);
    }
    map<uint64_t, stateMachine*>::iterator iter;
    for (iter = t.net->peers.begin(); iter != t.net->peers.end(); ++iter) {
      raft *r = (raft*)iter->second->data();
      EXPECT_EQ(r->raftLog_->committed_, t.wcommitted);
      
      EntryVec entries, ents;
      nextEnts(r, t.net->storage[iter->first], &entries);
      int m;
      for (m = 0; m < entries.size(); ++m) {
        if (entries[m].has_data()) {
          ents.push_back(entries[m]);
        }
      }

      vector<Message*> props;
      for (m = 0; m < t.msgs.size(); ++m) {
        if (t.msgs[m]->type() == MsgProp) {
          props.push_back(t.msgs[m]);
        }
      } 
      for (m = 0; m < props.size(); ++m) {
        Message *msg = props[m];
        kDefaultLogger.Infof(__FILE__, __LINE__, "m:%d, size:%d", props.size(), msg->entries_size());
        //const Entry& entry = msg->entries(0);
        kDefaultLogger.Infof(__FILE__, __LINE__, "m:%d, data:%s", m, ents[m].data().c_str());
        EXPECT_TRUE(ents[m].data() == msg->entries(0).data());
      }
    }
  }
}
