/*
 * Copyright (C) lichuang
 */

#include <gtest/gtest.h>
#include <math.h>
#include "libraft.h"
#include "raft_test_util.h"
#include "base/logger.h"
#include "base/util.h"
#include "core/raft.h"
#include "core/progress.h"
#include "core/read_only.h"
#include "storage/memory_storage.h"

using namespace libraft;

extern stateMachine *nopStepper;

/*
This file contains tests which verify that the scenarios described
in the raft paper (https://ramcloud.stanford.edu/raft.pdf) are
handled by the raft implementation correctly. Each test focuses on
several sentences written in the paper. This could help us to prevent
most implementation bugs.

Each test is composed of three parts: init, test and check.
Init part uses simple and understandable way to simulate the init state.
Test part uses Step function to generate the scenario. Check part checks
outgoing messages and state.
*/

static void
releaseMsgVector(MessageVec* mvec) {
  uint32_t i;
  for (i = 0; i < mvec->size(); ++i) {
    delete (*mvec)[i];
  }
}

bool isDeepEqualMsgs(const MessageVec& msgs1, const MessageVec& msgs2) {
	if (msgs1.size() != msgs2.size()) {
    Debugf("error");
		return false;
	}
	size_t i;
	for (i = 0; i < msgs1.size(); ++i) {
		Message *m1 = msgs1[i];
		Message *m2 = msgs2[i];
		if (m1->from() != m2->from()) {
      Debugf("error");
			return false;
		}
		if (m1->to() != m2->to()) {
      Debugf("m1 to %llu, m2 to %llu", m1->to(), m2->to());
			return false;
		}
		if (m1->term() != m2->term()) {
      Debugf("error");
			return false;
		}
		if (m1->logterm() != m2->logterm()) {
      Debugf("error");
			return false;
		}
		if (m1->index() != m2->index()) {
      Debugf("error");
			return false;
		}
		if (m1->commit() != m2->commit()) {
      Debugf("error");
			return false;
		}
		if (m1->type() != m2->type()) {
      Debugf("error");
			return false;
		}
		if (m1->reject() != m2->reject()) {
      Debugf("error");
			return false;
		}
		if (m1->entries_size() != m2->entries_size()) {
      Debugf("error");
			return false;
		}
	}
	return true;
}

Message acceptAndReply(Message *msg) {
  EXPECT_EQ(msg->type(), MsgApp);

  Message m;
  m.set_from(msg->to());
  m.set_to(msg->from());
  m.set_term(msg->term());
  m.set_type(MsgAppResp);
  m.set_index(msg->index() + msg->entries_size());
  return m;
}

void commitNoopEntry(raft *r, MemoryStorage *s) {
  EXPECT_EQ(r->state_, StateLeader);
  r->bcastAppend();
  // simulate the response of MsgApp
  MessageVec msgs;
  r->readMessages(&msgs);

  size_t i;
  for (i = 0; i < msgs.size(); ++i) {
    Message *msg = msgs[i];
    EXPECT_FALSE(msg->type() != MsgApp || msg->entries_size() != 1 || msg->entries(0).has_data());
    r->step(acceptAndReply(msg));
  }
  // ignore further messages to refresh followers' commit index
  r->readMessages(&msgs);
  EntryVec entries;
  r->raftLog_->unstableEntries(&entries);
  s->Append(entries);
  r->raftLog_->appliedTo(r->raftLog_->committed_);
  r->raftLog_->stableTo(r->raftLog_->lastIndex(), r->raftLog_->lastTerm());
}

// testUpdateTermFromMessage tests that if one server’s current term is
// smaller than the other’s, then it updates its current term to the larger
// value. If a candidate or leader discovers that its term is out of date,
// it immediately reverts to follower state.
// Reference: section 5.1
void testUpdateTermFromMessage(StateType state) {
  vector<uint64_t> peers = {1,2,3};
  Storage *s = new MemoryStorage(NULL);
  raft *r = newTestRaft(1, peers, 10, 1, s);

	switch (state) {
	case StateFollower:
		r->becomeFollower(1,2);
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

  {
    Message msg;
    msg.set_type(MsgApp);
    msg.set_term(2);

    r->step(msg);
  }

	EXPECT_EQ((int)r->term_, 2);
	EXPECT_EQ(r->state_, StateFollower);

  delete r;
}

TEST(raftPaperTests, TestFollowerUpdateTermFromMessage) {
	testUpdateTermFromMessage(StateFollower);
}

TEST(raftPaperTests, TestCandidateUpdateTermFromMessage) {
	testUpdateTermFromMessage(StateCandidate);
}

TEST(raftPaperTests, TestLeaderUpdateTermFromMessage) {
	testUpdateTermFromMessage(StateLeader);
}

// TestRejectStaleTermMessage tests that if a server receives a request with
// a stale term number, it rejects the request.
// Our implementation ignores the request instead.
// Reference: section 5.1
// TODO
static bool called = false;

static void 
fakeStep(raft* r, const Message& msg) {
  called = true;
}

TEST(raftPaperTests, TestRejectStaleTermMessage) {
  called = false;
  vector<uint64_t> peers = {1,2,3};
  Storage *s = new MemoryStorage(NULL);
  raft *r = newTestRaft(1, peers, 10, 1, s);
  r->stateStepFunc_ = fakeStep;

  HardState hs;
  hs.set_term(2);
  r->loadState(hs);

  {
    Message msg;
    msg.set_type(MsgApp);
    msg.set_term(r->term_ - 1);

    r->step(msg);
  }

  EXPECT_FALSE(called);
  delete r;
}

// TestStartAsFollower tests that when servers start up, they begin as followers.
// Reference: section 5.2
TEST(raftPaperTests, TestStartAsFollower) {
  vector<uint64_t> peers = {1,2,3};
  Storage *s = new MemoryStorage(NULL);
  raft *r = newTestRaft(1, peers, 10, 1, s);
	EXPECT_EQ(r->state_, StateFollower);

  delete r;
}

// TestLeaderBcastBeat tests that if the leader receives a heartbeat tick,
// it will send a msgApp with m.Index = 0, m.LogTerm=0 and empty entries as
// heartbeat to all followers.
// Reference: section 5.2
TEST(raftPaperTests, TestLeaderBcastBeat) {
	// heartbeat interval
	uint64_t hi = 1;
  vector<uint64_t> peers = {1,2,3};
  Storage *s = new MemoryStorage(NULL);
  raft *r = newTestRaft(1, peers, 10, 1, s);
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

  for (i = 0; i < hi; ++i) {
		r->tick();
	}
  MessageVec msgs;
  r->readMessages(&msgs);

	MessageVec wmsgs;
	{
		Message *msg = new Message();
		msg->set_from(1);
		msg->set_to(2);
		msg->set_term(1);
		msg->set_type(MsgHeartbeat);
		wmsgs.push_back(msg);
	}
	{
		Message *msg = new Message();
		msg->set_from(1);
		msg->set_to(3);
		msg->set_term(1);
		msg->set_type(MsgHeartbeat);
		wmsgs.push_back(msg);
	}
	EXPECT_TRUE(isDeepEqualMsgs(msgs, wmsgs));

  delete r;
  releaseMsgVector(&wmsgs);
}

// testNonleaderStartElection tests that if a follower receives no communication
// over election timeout, it begins an election to choose a new leader. It
// increments its current term and transitions to candidate state. It then
// votes for itself and issues RequestVote RPCs in parallel to each of the
// other servers in the cluster.
// Reference: section 5.2
// Also if a candidate fails to obtain a majority, it will time out and
// start a new election by incrementing its term and initiating another
// round of RequestVote RPCs.
// Reference: section 5.2
void testNonleaderStartElection(StateType state) {
	// election timeout
	uint64_t et = 10;
  vector<uint64_t> peers = {1,2,3};

  Storage *s = new MemoryStorage(NULL);
  raft *r = newTestRaft(1, peers, 10, 1, s);

	switch (state) {
	case StateFollower:
		r->becomeFollower(1,2);
		break;
	case StateCandidate:
		r->becomeCandidate();
		break;
  default:
    break;
	}

  // simulate election timeout
	size_t i;
  for (i = 0; i < 2*et; ++i) {
		r->tick();
	}

	EXPECT_EQ((int)r->term_, 2);
	EXPECT_EQ(r->state_, StateCandidate);
	EXPECT_TRUE(r->votes_[r->id_]);
  MessageVec msgs;
  r->readMessages(&msgs);

	MessageVec wmsgs;
	{
		Message *msg = new Message();
		msg->set_from(1);
		msg->set_to(2);
		msg->set_term(2);
		msg->set_type(MsgVote);
		wmsgs.push_back(msg);
	}
	{
		Message *msg = new Message();
		msg->set_from(1);
		msg->set_to(3);
		msg->set_term(2);
		msg->set_type(MsgVote);
		wmsgs.push_back(msg);
	}
	EXPECT_TRUE(isDeepEqualMsgs(msgs, wmsgs));

  delete r;  
  releaseMsgVector(&wmsgs);
}

TEST(raftPaperTests, TestFollowerStartElection) {
	testNonleaderStartElection(StateFollower);
}

TEST(raftPaperTests, TestCandidateStartNewElection) {
	testNonleaderStartElection(StateCandidate);
}

// TestLeaderElectionInOneRoundRPC tests all cases that may happen in
// leader election during one round of RequestVote RPC:
// a) it wins the election
// b) it loses the election
// c) it is unclear about the result
// Reference: section 5.2
TEST(raftPaperTests, TestLeaderElectionInOneRoundRPC) {  
	struct tmp {
		int size;
		map<uint64_t, bool> votes;
		StateType state;
	} tests[] = {
    // win the election when receiving votes from a majority of the servers
    {.size = 1, .votes = {}, .state = StateLeader},
    {.size = 3, .votes = {{2,true},{3,true}}, .state = StateLeader},
    {.size = 3, .votes = {{2,true},}, .state = StateLeader},
    {.size = 5, .votes = {{2,true},{3,true},{4,true},{5,true},}, .state = StateLeader},
    {.size = 5, .votes = {{2,true},{3,true},{4,true},}, .state = StateLeader},
    {.size = 5, .votes = {{2,true},{3,true},}, .state = StateLeader},

    // return to follower state if it receives vote denial from a majority
    {.size = 3, .votes = {{2,false},{3,false},}, .state = StateFollower},
    {.size = 5, .votes = {{2,false},{3,false},{4,false},{5,false},}, .state = StateFollower},
    {.size = 5, .votes = {{2,true},{3,false},{4,false},{5,false},}, .state = StateFollower},

    // stay in candidate if it does not obtain the majority
    {.size = 3, .votes = {}, .state = StateCandidate},
    {.size = 5, .votes = {{2,true},}, .state = StateCandidate},
    {.size = 5, .votes = {{2,false},{3,false}}, .state = StateCandidate},
    {.size = 5, .votes = {}, .state = StateCandidate},
  };

	size_t i;
	for (i = 0; i < SIZEOF_ARRAY(tests); ++i) {
		tmp &t = tests[i];

		vector<uint64_t> peers;
		idsBySize(t.size, &peers);
		Storage *s = new MemoryStorage(NULL);
		raft *r = newTestRaft(1, peers, 10, 1, s);

		r->step(initMessage(1,1,MsgHup));

		map<uint64_t, bool>::iterator iter;
		for (iter = t.votes.begin(); iter != t.votes.end(); ++iter) {
			Message msg = initMessage(iter->first,1,MsgVoteResp);
			msg.set_reject(!iter->second);
			r->step(msg);
		}

		EXPECT_EQ(r->state_, t.state) << "i: " << i;
		EXPECT_EQ((int)r->term_, 1);

    delete r;
	}
}

// TestFollowerVote tests that each follower will vote for at most one
// candidate in a given term, on a first-come-first-served basis.
// Reference: section 5.2
TEST(raftPaperTests, TestFollowerVote) {
	struct tmp {
    uint64_t vote;
    uint64_t nvote;
    bool wreject;
	} tests[] = {
    {.vote = kEmptyPeerId, .nvote = 1, .wreject = false},
    {.vote = kEmptyPeerId, .nvote = 2, .wreject = false},
    {.vote = 1, .nvote = 1, .wreject = false},
    {.vote = 2, .nvote = 2, .wreject = false},
    {.vote = 1, .nvote = 2, .wreject = true},
    {.vote = 2, .nvote = 1, .wreject = true},
  };

  size_t i;
  for (i = 0; i < SIZEOF_ARRAY(tests); ++i) {
    tmp &t = tests[i];
    
    vector<uint64_t> peers = {1,2,3};
    Storage *s = new MemoryStorage(NULL);
    raft *r = newTestRaft(1, peers, 10, 1, s);

    HardState hs;
    hs.set_term(1);
    hs.set_vote(t.vote);
    r->loadState(hs);

		r->step(initMessage(t.nvote,1,MsgVote));

    MessageVec msgs;
    r->readMessages(&msgs);

    MessageVec wmsgs;
    {
      Message *msg = new Message();
      msg->set_from(1);
      msg->set_to(t.nvote);
      msg->set_term(1);
      msg->set_type(MsgVoteResp);
      msg->set_reject(t.wreject);
      wmsgs.push_back(msg);
    }
	  EXPECT_TRUE(isDeepEqualMsgs(msgs, wmsgs));

    delete r;
    releaseMsgVector(&wmsgs);
  }
}

// TestCandidateFallback tests that while waiting for votes,
// if a candidate receives an AppendEntries RPC from another server claiming
// to be leader whose term is at least as large as the candidate's current term,
// it recognizes the leader as legitimate and returns to follower state.
// Reference: section 5.2
TEST(raftPaperTests, TestCandidateFallback) {
  vector<Message> tests;

  {
    Message msg;
    msg.set_from(2);
    msg.set_to(1);
    msg.set_term(1);
    msg.set_type(MsgApp);
    tests.push_back(msg);
  }
  {
    Message msg;
    msg.set_from(2);
    msg.set_to(1);
    msg.set_term(2);
    msg.set_type(MsgApp);
    tests.push_back(msg);
  }
  size_t i;
  for (i = 0; i < tests.size(); ++i) {
    Message& msg = tests[i];
    
    vector<uint64_t> peers = {1,2,3};
    Storage *s = new MemoryStorage(NULL);
    raft *r = newTestRaft(1, peers, 10, 1, s);

		r->step(initMessage(1,1,MsgHup));
		
    EXPECT_EQ(r->state_, StateCandidate);
    r->step(msg);

    EXPECT_EQ(r->state_, StateFollower);
    EXPECT_EQ(r->term_, msg.term());
    delete r;
  }
}

// testNonleaderElectionTimeoutRandomized tests that election timeout for
// follower or candidate is randomized.
// Reference: section 5.2
void testNonleaderElectionTimeoutRandomized(StateType state) {
  uint64_t et = 10;
  
  vector<uint64_t> peers = {1,2,3};

  Storage *s = new MemoryStorage(NULL);
  raft *r = newTestRaft(1, peers, et, 1, s);
  size_t i;
  map<int, bool> timeouts;
  for (i = 0; i < 50 * et; ++i) {
    switch (state) {
    case StateFollower:
      r->becomeFollower(r->term_ + 1,2);
      break;
    case StateCandidate:
      r->becomeCandidate();
      break;
    default:
      break;
    }

    uint64_t time = 0;
    MessageVec msgs;
    r->readMessages(&msgs);
    while (msgs.size() == 0) {
      r->tick();
      time++;
      r->readMessages(&msgs);
    }
    timeouts[time] = true;
  }

  for (i = et + 1; i < 2 * et; ++i) {
    EXPECT_TRUE(timeouts[i]);
  }

  delete r;
}

TEST(raftPaperTests, TestFollowerElectionTimeoutRandomized) {
  testNonleaderElectionTimeoutRandomized(StateFollower);
}

TEST(raftPaperTests, TestCandidateElectionTimeoutRandomized) {
  testNonleaderElectionTimeoutRandomized(StateCandidate);
}

// testNonleadersElectionTimeoutNonconflict tests that in most cases only a
// single server(follower or candidate) will time out, which reduces the
// likelihood of split vote in the new election.
// Reference: section 5.2
void testNonleadersElectionTimeoutNonconflict(StateType state) {
  uint64_t et = 10;
  int size = 5;
  vector<raft*> rs;
  vector<uint64_t> peers;
  idsBySize(size, &peers);
  size_t i;
  for (i = 0; i < peers.size(); ++i) {
    Storage *s = new MemoryStorage(NULL);
    raft *r = newTestRaft(peers[i], peers, et, 1, s);
    rs.push_back(r);
  }
  int conflicts = 0;
  for (i = 0; i < 1000; ++i) {
    size_t j;
    for (j = 0; j < rs.size(); ++j) {
      raft *r = rs[j];
      switch (state) {
      case StateFollower:
        r->becomeFollower(r->term_ + 1,kEmptyPeerId);
        break;
      case StateCandidate:
        r->becomeCandidate();
        break;
      default:
        break;        
      }
    }

    int timeoutNum = 0;
    while (timeoutNum == 0) {
      for (j = 0; j < rs.size(); ++j) {
        raft *r = rs[j];
        r->tick();
        MessageVec msgs;
        r->readMessages(&msgs);
        if (msgs.size() > 0) {
          ++timeoutNum;
        }
      }
    }

    // several rafts time out at the same tick
    if (timeoutNum > 1) {
      ++conflicts;
    }
  }

  float g = float(conflicts) / 1000;
  EXPECT_FALSE(g > 0.3);

  for (i = 0; i < rs.size(); ++i) {
    delete rs[i];
  }  
}

TEST(raftPaperTests, TestFollowersElectioinTimeoutNonconflict) {
  testNonleadersElectionTimeoutNonconflict(StateFollower);
}

TEST(raftPaperTests, TestCandidatesElectionTimeoutNonconflict) {
  testNonleadersElectionTimeoutNonconflict(StateCandidate);
}

// TestLeaderStartReplication tests that when receiving client proposals,
// the leader appends the proposal to its log as a new entry, then issues
// AppendEntries RPCs in parallel to each of the other servers to replicate
// the entry. Also, when sending an AppendEntries RPC, the leader includes
// the index and term of the entry in its log that immediately precedes
// the new entries.
// Also, it writes the new entry into stable storage.
// Reference: section 5.3
TEST(raftPaperTests, TestLeaderStartReplication) {
  vector<uint64_t> peers = {1,2,3};
  MemoryStorage *s = new MemoryStorage(NULL);
  raft *r = newTestRaft(1, peers, 10, 1, s);
  r->becomeCandidate();
  r->becomeLeader();

  commitNoopEntry(r, s);
  uint64_t li = r->raftLog_->lastIndex();
  
  {
    Entry entry;
    entry.set_data("some data");
    Message msg = initMessage(1,1,MsgProp);
    *(msg.add_entries()) = entry;
    r->step(msg);
  }

  EXPECT_EQ(r->raftLog_->lastIndex(), li + 1);
  EXPECT_EQ(r->raftLog_->committed_, li);

  MessageVec msgs, wmsgs;
  r->readMessages(&msgs);

  Entry entry;
  entry.set_index(li + 1);
  entry.set_term(1);
  entry.set_data("some data");
  EntryVec g, wents;
  wents.push_back(entry);
  r->raftLog_->unstableEntries(&g);
  EXPECT_TRUE(isDeepEqualEntries(g, wents));
  {
    Message *msg = new Message();
    msg->set_from(1);
    msg->set_to(2);
    msg->set_term(1);
    msg->set_type(MsgApp);
    msg->set_index(li);
    msg->set_logterm(1);
    msg->set_commit(li);
    *(msg->add_entries()) = entry;
    wmsgs.push_back(msg);
  }
  {
    Message *msg = new Message();
    msg->set_from(1);
    msg->set_to(3);
    msg->set_term(1);
    msg->set_type(MsgApp);
    msg->set_index(li);
    msg->set_logterm(1);
    msg->set_commit(li);
    *(msg->add_entries()) = entry;
    wmsgs.push_back(msg);
  }
  EXPECT_TRUE(isDeepEqualMsgs(msgs, wmsgs));

  delete r;
  releaseMsgVector(&wmsgs);
}

// TestLeaderCommitEntry tests that when the entry has been safely replicated,
// the leader gives out the applied entries, which can be applied to its state
// machine.
// Also, the leader keeps track of the highest index it knows to be committed,
// and it includes that index in future AppendEntries RPCs so that the other
// servers eventually find out.
// Reference: section 5.3
TEST(raftPaperTests, TestLeaderCommitEntry) {
  vector<uint64_t> peers = {1,2,3};

  MemoryStorage *s = new MemoryStorage(NULL);
  raft *r = newTestRaft(1, peers, 10, 1, s);
  r->becomeCandidate();
  r->becomeLeader();

  commitNoopEntry(r, s);
  uint64_t li = r->raftLog_->lastIndex();
  
  {
    Entry entry;
    entry.set_data("some data");
    Message msg = initMessage(1,1,MsgProp);
    *(msg.add_entries()) = entry;
    r->step(msg);
  }

  MessageVec msgs;
  r->readMessages(&msgs);
  size_t i;
  for (i = 0; i < msgs.size(); ++i) {
    Message *msg = msgs[i];
    r->step(acceptAndReply(msg));
  }

  EXPECT_EQ(r->raftLog_->committed_, li + 1);  

  Entry entry = initEntry(li + 1, 1, "some data");
  EntryVec g, wents;
  wents.push_back(entry);
  r->raftLog_->nextEntries(&g);
  EXPECT_TRUE(isDeepEqualEntries(g,wents));

  r->readMessages(&msgs);
  for (i = 0; i < msgs.size(); ++i) {
    Message *msg = msgs[i];
    EXPECT_EQ(msg->to(), i + 2);
    EXPECT_EQ(msg->type(), MsgApp);
    EXPECT_EQ(msg->commit(), li +1);
  }

  delete r;
}

// TestLeaderAcknowledgeCommit tests that a log entry is committed once the
// leader that created the entry has replicated it on a majority of the servers.
// Reference: section 5.3
TEST(raftPaperTests, TestLeaderAcknowledgeCommit) {
  struct tmp {
    int size;
    map<uint64_t, bool> acceptors;
    bool wack;
  } tests[] = {
    { .size = 1, .acceptors = {}, .wack = true},
    { .size = 3, .acceptors = {}, .wack = false},
    { .size = 3, .acceptors = {{2,true}}, .wack = true},
    { .size = 3, .acceptors = {{2,true},{3,true}}, .wack = true},
    { .size = 5, .acceptors = {}, .wack = false},
    { .size = 5, .acceptors = {{2,true},}, .wack = false},
    { .size = 5, .acceptors = {{2,true},{3,true}}, .wack = true},
    { .size = 5, .acceptors = {{2,true},{3,true},{4,true}}, .wack = true},
    { .size = 5, .acceptors = {{2,true},{3,true},{4,true},{5,true}}, .wack = true},
  };

  size_t i;
  for (i = 0; i < SIZEOF_ARRAY(tests); ++i) {
    tmp &t = tests[i];
    vector<uint64_t> peers;
    idsBySize(t.size, &peers);
    MemoryStorage *s = new MemoryStorage(NULL);
    raft *r = newTestRaft(1, peers, 10, 1, s);
		r->becomeCandidate();
		r->becomeLeader();
    commitNoopEntry(r, s);
    uint64_t li = r->raftLog_->lastIndex();

    {
      Entry entry;
      entry.set_data("some data");
      Message msg = initMessage(1,1,MsgProp);
      *(msg.add_entries()) = entry;
      r->step(msg);
    }
    MessageVec msgs;
    r->readMessages(&msgs);
    size_t j;
    for (j = 0; j < msgs.size(); ++j) {
      Message *msg = msgs[j];
      if (t.acceptors[msg->to()]) {
        r->step(acceptAndReply(msg));
      }
    }

    EXPECT_EQ(r->raftLog_->committed_ > li, t.wack);

    delete r;
  }
}

// TestLeaderCommitPrecedingEntries tests that when leader commits a log entry,
// it also commits all preceding entries in the leader’s log, including
// entries created by previous leaders.
// Also, it applies the entry to its local state machine (in log order).
// Reference: section 5.3
TEST(raftPaperTests, TestLeaderCommitPrecedingEntries) {
  EntryVec tests[] = {
    {},
    {initEntry(1,2)},
    {initEntry(1,1),initEntry(2,2)},
    {initEntry(1,1),},
  };

  size_t i;
  for (i = 0; i < SIZEOF_ARRAY(tests); ++i) {
    EntryVec &t = tests[i];
    vector<uint64_t> peers = {1,2,3};

    MemoryStorage *s = new MemoryStorage(NULL);
    EntryVec appEntries = t;
    s->Append(appEntries);
    raft *r = newTestRaft(1, peers, 10, 1, s);

    HardState hs;
    hs.set_term(2);
    r->loadState(hs);
		r->becomeCandidate();
		r->becomeLeader();

    {
      Entry entry;
      entry.set_data("some data");
      Message msg = initMessage(1,1,MsgProp);
      *(msg.add_entries()) = entry;
      r->step(msg);
    }

    MessageVec msgs;
    r->readMessages(&msgs);
    size_t j;
    for (j = 0; j < msgs.size(); ++j) {
      Message *msg = msgs[j];
      r->step(acceptAndReply(msg));
    }

    uint64_t li = t.size();
    EntryVec g, wents = t;

    wents.push_back(initEntry(li + 1, 3));
    wents.push_back(initEntry(li + 2, 3, "some data"));

    r->raftLog_->nextEntries(&g);
    EXPECT_TRUE(isDeepEqualEntries(g, wents)) << "i:" << i;

    delete r;
  }
}

// TestFollowerCommitEntry tests that once a follower learns that a log entry
// is committed, it applies the entry to its local state machine (in log order).
// Reference: section 5.3
TEST(raftPaperTests, TestFollowerCommitEntry) {
  struct tmp {
    EntryVec entries;
    uint64_t commit;
  } tests[] = {
    {.entries = {initEntry(1,1,"some data")}, .commit = 1},
    {.entries = {initEntry(1,1,"some data"),initEntry(2,1,"some data2")}, .commit = 2},
    {.entries = {initEntry(1,1,"some data2"),initEntry(2,1,"some data")}, .commit = 2},
    {.entries = {initEntry(1,1,"some data"),initEntry(2,1,"some data2")}, .commit = 1},
  };

  size_t i;
  for (i = 0; i < SIZEOF_ARRAY(tests); ++i) {
    tmp &t = tests[i];

    vector<uint64_t> peers = {1,2,3};

    Storage *s = new MemoryStorage(NULL);
    raft *r = newTestRaft(1, peers, 10, 1, s);
    r->becomeFollower(1,2);

    {
      Message msg = initMessage(2,1,MsgApp);
      msg.set_term(1);      
      msg.set_commit(t.commit);
      size_t j;
      for (j = 0; j < t.entries.size(); ++j) {
        *(msg.add_entries()) = t.entries[j];
      }
      r->step(msg);
    }

    EXPECT_EQ(r->raftLog_->committed_, t.commit);
    EntryVec ents, wents;
    r->raftLog_->nextEntries(&ents);
    wents.insert(wents.end(), t.entries.begin(), t.entries.begin() + t.commit);
    EXPECT_TRUE(isDeepEqualEntries(ents, wents)) << "i:" << i;

    delete r;
  }
}

// TestFollowerCheckMsgApp tests that if the follower does not find an
// entry in its log with the same index and term as the one in AppendEntries RPC,
// then it refuses the new entries. Otherwise it replies that it accepts the
// append entries.
// Reference: section 5.3
TEST(raftPaperTests, TestFollowerCheckMsgApp) {
  EntryVec entries = {
    initEntry(1,1),
    initEntry(2,2),
  };

  struct tmp {
    uint64_t term;
    uint64_t index;
    uint64_t windex;
    bool wreject;
    uint64_t wrejectHint;
  } tests[] = {
    // match with committed entries
    {.term = 0, .index = 0, .windex = 1, .wreject = false, .wrejectHint = 0},
    {.term = entries[0].term(), .index = entries[0].index(), .windex = 1, .wreject = false, .wrejectHint = 0},
    // match with uncommitted entries
    {.term = entries[1].term(), .index = entries[1].index(), .windex = 2, .wreject = false, .wrejectHint = 0},
    // unmatch with existing entry
    {.term = entries[0].term(), .index = entries[1].index(), .windex = entries[1].index(), .wreject = true, .wrejectHint = 2},
    // unexisting entry
    {.term = entries[1].term(), .index = entries[1].index() + 1, .windex = entries[1].index() + 1, .wreject = true, .wrejectHint = 2},
  };

  size_t i;
  for (i = 0; i < SIZEOF_ARRAY(tests); ++i) {
    tmp& t = tests[i];

    vector<uint64_t> peers = {1,2,3};

    MemoryStorage *s = new MemoryStorage(NULL);
    EntryVec ents = entries;
    s->Append(ents);
    raft *r = newTestRaft(1, peers, 10, 1, s);

    HardState hs;
    hs.set_commit(1);
    r->loadState(hs);
    r->becomeFollower(2, 2);
    {
      Message msg;
      msg.set_index(t.index);
      msg.set_type(MsgApp);
      msg.set_from(2);
      msg.set_to(1);
      msg.set_term(2);
      msg.set_logterm(t.term);      

      r->step(msg);
    }

    vector<Message *> msgs, wmsgs;
    r->readMessages(&msgs);
    {
      Message *msg = new Message();
      msg->set_from(1);
      msg->set_to(2);
      msg->set_type(MsgAppResp);
      msg->set_term(2);
      msg->set_index(t.windex);
      msg->set_reject(t.wreject);
      msg->set_rejecthint(t.wrejectHint);
      wmsgs.push_back(msg);
    }

    EXPECT_TRUE(isDeepEqualMsgs(msgs, wmsgs)) << "i: " << i;

    delete r;
    releaseMsgVector(&wmsgs);
  }
}

// TestFollowerAppendEntries tests that when AppendEntries RPC is valid,
// the follower will delete the existing conflict entry and all that follow it,
// and append any new entries not already in the log.
// Also, it writes the new entry into stable storage.
// Reference: section 5.3
TEST(raftPaperTests, TestFollowerAppendEntries) {
	struct tmp {
		uint64_t index, term;
		EntryVec ents, wents, wunstable;
	} tests[] = {
    {
      .index = 2, .term = 2,
      .ents = {initEntry(3,3)},
      .wents = {initEntry(1,1),initEntry(2,2),initEntry(3,3)},
      .wunstable = {initEntry(3,3)},
    },
    {
      .index = 1, .term = 1,
      .ents = {initEntry(2,3),initEntry(3,4)},
      .wents = {initEntry(1,1),initEntry(2,3),initEntry(3,4)},
      .wunstable = {initEntry(2,3),initEntry(3,4)},
    },
    {
      .index = 0, .term = 0,
      .ents = {initEntry(1,3),},
      .wents = {initEntry(1,3)},
      .wunstable = {initEntry(1,3)},
    },        
  };

	size_t i;
	for (i = 0; i < SIZEOF_ARRAY(tests); ++i) {
		tmp& t = tests[i];
		vector<uint64_t> peers = {1,2,3};

		MemoryStorage *s = new MemoryStorage(NULL);

		EntryVec appEntries = {
      initEntry(1,1),
      initEntry(2,2),
    };

    s->Append(appEntries);
		raft *r = newTestRaft(1, peers, 10, 1, s);
		r->becomeFollower(2, 2);
    {
      Message msg;
      msg.set_type(MsgApp);
      msg.set_from(2);
      msg.set_to(1);
      msg.set_term(2);
      msg.set_logterm(t.term);
      msg.set_index(t.index);
			size_t j;
			for (j = 0; j < t.ents.size(); ++j) {
				*(msg.add_entries()) = t.ents[j];
			}

      r->step(msg);
    }

		EntryVec wents, wunstable;
		r->raftLog_->allEntries(&wents);
		EXPECT_TRUE(isDeepEqualEntries(wents, t.wents));

		r->raftLog_->unstableEntries(&wunstable);
		EXPECT_TRUE(isDeepEqualEntries(wunstable, t.wunstable));

    delete r;
	}
}

// TestLeaderSyncFollowerLog tests that the leader could bring a follower's log
// into consistency with its own.
// Reference: section 5.3, figure 7
TEST(raftPaperTests, TestLeaderSyncFollowerLog) {
	EntryVec ents = {
    {},
    initEntry(1,1),initEntry(2,1),initEntry(3,1),
    initEntry(4,4),initEntry(5,4),
    initEntry(6,5),initEntry(7,5),
    initEntry(8,6),initEntry(9,6),initEntry(10,6),
  };	

	uint64_t term = 8;
	vector<EntryVec> tests = {
    {
      {},
      initEntry(1,1),initEntry(2,1),initEntry(3,1),
      initEntry(4,4),initEntry(5,4),
      initEntry(6,5),initEntry(7,5),
      initEntry(8,6),initEntry(9,6),   
    },
    {
      {},
      initEntry(1,1),initEntry(2,1),initEntry(3,1),
      initEntry(4,4),   
    }, 
    {
      {},
      initEntry(1,1),initEntry(2,1),initEntry(3,1),
      initEntry(4,4),initEntry(5,4),
      initEntry(6,5),initEntry(7,5),
      initEntry(8,6),initEntry(9,6),initEntry(10,6),initEntry(11,6),   
    }, 
    {
      {},
      initEntry(1,1),initEntry(2,1),initEntry(3,1),
      initEntry(4,4),initEntry(5,4),
      initEntry(6,5),initEntry(7,5),
      initEntry(8,6),initEntry(9,6),initEntry(10,6), 
      initEntry(11,7),initEntry(12,7),
    },
    {
      {},
      initEntry(1,1),initEntry(2,1),initEntry(3,1),
      initEntry(4,4),initEntry(5,4),initEntry(6,4),initEntry(7,4),
    },   
    {
      {},
      initEntry(1,1),initEntry(2,1),initEntry(3,1),
      initEntry(4,2),initEntry(5,2),initEntry(6,2),
      initEntry(7,3),initEntry(8,3),initEntry(9,3),initEntry(10,3),initEntry(11,3),
    },                 
  };

	size_t i;
	for (i = 0; i < tests.size(); ++i) {
		EntryVec& t = tests[i];

		vector<uint64_t> peers = {1,2,3};

		MemoryStorage *leaderStorage = new MemoryStorage(NULL);
		EntryVec appEntries = ents;
    leaderStorage->Append(appEntries);
		raft *leader = newTestRaft(1, peers, 10, 1, leaderStorage);

    {
      HardState hs;
      hs.set_commit(leader->raftLog_->lastIndex());
      hs.set_term(term);
      leader->loadState(hs);
    }

		MemoryStorage *followerStorage = new MemoryStorage(NULL);
    followerStorage->Append(t);
		raft *follower = newTestRaft(2, peers, 10, 1, followerStorage);

    {
      HardState hs;
      hs.set_term(term - 1);
      follower->loadState(hs);
    }
		// It is necessary to have a three-node cluster.
		// The second may have more up-to-date log than the first one, so the
		// first node needs the vote from the third node to become the leader.
		vector<stateMachine*> sts = {new raftStateMachine(leader), new raftStateMachine(follower), nopStepper};
		
  	network *net = newNetwork(sts);
    {
      vector<Message> msgs = {initMessage(1,1,MsgHup)};
      net->send(&msgs);
    }
		// The election occurs in the term after the one we loaded with
		// lead.loadState above.
    {
      vector<Message> msgs;
      Message msg = initMessage(3,1,MsgVoteResp);

      msg.set_term(term + 1);
      msgs.push_back(msg);
      net->send(&msgs);
    }
    {
      vector<Message> msgs;
      Message msg = initMessage(1,1,MsgProp);
			msg.add_entries();
      msgs.push_back(msg);
      net->send(&msgs);
    }

		EXPECT_EQ(raftLogString(leader->raftLog_), raftLogString(follower->raftLog_)) << "i: " << i;

    delete leader;
    delete follower;
	}
}

// TestVoteRequest tests that the vote request includes information about the candidate’s log
// and are sent to all of the other nodes.
// Reference: section 5.4.1
TEST(raftPaperTests, TestVoteRequest) {
  struct tmp {
    EntryVec ents;
    uint64_t wterm;
  } tests[] = {
    {
      .ents = {initEntry(1,1)},
      .wterm = 2,
    },
    {
      .ents = {initEntry(1,1), initEntry(2,2),},
      .wterm = 3,
    },    
  };

  size_t i;
  for (i = 0; i < SIZEOF_ARRAY(tests); ++i) {
    tmp & t = tests[i];
  
    vector<uint64_t> peers = {1,2,3};

    Storage *s = new MemoryStorage(NULL);
    raft *r = newTestRaft(1, peers, 10, 1, s);
    {
      Message msg = initMessage(2,1,MsgApp);
      msg.set_term(t.wterm - 1);
      msg.set_logterm(0);
      msg.set_index(0);
      size_t j;
      for (j = 0; j < t.ents.size(); ++j) {
        *(msg.add_entries()) = t.ents[j];
      }
      r->step(msg);
    }

    MessageVec msgs;
    r->readMessages(&msgs);
    
    size_t j;
    for (j = 0; (int)j < r->electionTimeout_ * 2; ++j) {
      r->tickElection();
    }
    r->readMessages(&msgs);
    EXPECT_EQ((int)msgs.size(), 2);
    
    for (j = 0; j < msgs.size(); ++j) {
      Message *msg = msgs[j];
      
      EXPECT_EQ(msg->type(), MsgVote);
      EXPECT_EQ(msg->to(), j + 2);
      EXPECT_EQ(msg->term(), t.wterm);

      uint64_t windex = t.ents[t.ents.size() - 1].index();
      uint64_t wlogterm = t.ents[t.ents.size() - 1].term();

      EXPECT_EQ(msg->index(), windex);
      EXPECT_EQ(msg->logterm(), wlogterm);
    }

    delete r;
  }
}

// TestVoter tests the voter denies its vote if its own log is more up-to-date
// than that of the candidate.
// Reference: section 5.4.1
TEST(raftPaperTests, TestVoter) {
  struct tmp {
    EntryVec ents;
    uint64_t logterm;
    uint64_t index;
    bool wreject;
  } tests[] = {
    // same logterm
    {
      .ents = {initEntry(1,1)},
      .logterm = 1, .index = 1, .wreject = false,
    },
    {
      .ents = {initEntry(1,1)},
      .logterm = 1, .index = 2, .wreject = false,
    },
    {
      .ents = {initEntry(1,1),initEntry(2,1)},
      .logterm = 1, .index = 1, .wreject = true,
    },  
    // candidate higher logterm
    {
      .ents = {initEntry(1,1),},
      .logterm = 2, .index = 1, .wreject = false,
    },
    {
      .ents = {initEntry(1,1),},
      .logterm = 2, .index = 2, .wreject = false,
    },     
    {
      .ents = {initEntry(1,1),initEntry(2,1)},
      .logterm = 2, .index = 1, .wreject = false,
    },
    // voter higher logterm      
    {
      .ents = {initEntry(1,2),},
      .logterm = 1, .index = 1, .wreject = true,
    },
    {
      .ents = {initEntry(1,2),},
      .logterm = 1, .index = 2, .wreject = true,
    },     
    {
      .ents = {initEntry(1,2),initEntry(2,1)},
      .logterm = 1, .index = 1, .wreject = true,
    },
  };

  size_t i;
  for (i = 0; i < SIZEOF_ARRAY(tests); ++i) {
    tmp& t = tests[i];
    
    vector<uint64_t> peers = {1,2};

    MemoryStorage *s = new MemoryStorage(NULL);
    s->Append(t.ents);
    raft *r = newTestRaft(1, peers, 10, 1, s);

		{
			Message msg = initMessage(2,1,MsgVote);
			msg.set_term(3);
			msg.set_logterm(t.logterm);
			msg.set_index(t.index);
			r->step(msg);
		}

    MessageVec msgs;
    r->readMessages(&msgs);

    EXPECT_EQ((int)msgs.size(), 1);
    Message *msg = msgs[0];
    
    EXPECT_EQ(msg->type(), MsgVoteResp);
    EXPECT_EQ(msg->reject(), t.wreject);

    delete r;
  }
}

// TestLeaderOnlyCommitsLogFromCurrentTerm tests that only log entries from the leader’s
// current term are committed by counting replicas.
// Reference: section 5.4.2
TEST(raftPaperTests, TestLeaderOnlyCommitsLogFromCurrentTerm) {
  EntryVec entries = {
    initEntry(1,1),
    initEntry(2,2),
  }; 

  struct tmp {
    uint64_t index, wcommit;
  } tests[] = {
    // do not commit log entries in previous terms
    {.index = 1, .wcommit = 0},
    {.index = 2, .wcommit = 0},
    // commit log in current term
    {.index = 3, .wcommit = 3},
  };

  size_t i;
  for (i = 0; i < SIZEOF_ARRAY(tests); ++i) {
    tmp& t = tests[i];
    EntryVec ents = entries;

		MemoryStorage *s = new MemoryStorage(NULL);
    s->Append(ents);
		vector<uint64_t> peers = {1,2};
		raft *r = newTestRaft(1, peers, 10, 1, s);

    HardState hs;
    hs.set_term(3);
    r->loadState(hs);

    // become leader at term 3
    r->becomeCandidate();
    r->becomeLeader();

    MessageVec msgs;
    r->readMessages(&msgs);

    // propose a entry to current term
    {
      Entry entry;
      Message msg = initMessage(1,1,MsgProp);
      msg.add_entries();
      r->step(msg);
    }
    {
      Entry entry;
      Message msg = initMessage(2,1,MsgAppResp);

      msg.set_term(r->term_);
      msg.set_index(t.index);

      r->step(msg);
    }

    EXPECT_EQ(r->raftLog_->committed_, t.wcommit);

    delete r;
  }
}
