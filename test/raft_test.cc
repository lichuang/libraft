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
/*
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
*/
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
/*
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
*/
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

TEST(raftTests, TestLeaderElection) {
  //testLeaderElection(false);
}

TEST(raftTests, TestLeaderElectionPreVote) {
  testLeaderElection(true);
}
