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
    kDefaultLogger.Debugf(__FILE__, __LINE__, "error");
		return false;
	}
	size_t i;
	for (i = 0; i < msgs1.size(); ++i) {
		Message *m1 = msgs1[i];
		Message *m2 = msgs2[i];
		if (m1->from() != m2->from()) {
      kDefaultLogger.Debugf(__FILE__, __LINE__, "error");
			return false;
		}
		if (m1->to() != m2->to()) {
      kDefaultLogger.Debugf(__FILE__, __LINE__, "m1 to %llu, m2 to %llu", m1->to(), m2->to());
			return false;
		}
		if (m1->term() != m2->term()) {
      kDefaultLogger.Debugf(__FILE__, __LINE__, "error");
			return false;
		}
		if (m1->logterm() != m2->logterm()) {
      kDefaultLogger.Debugf(__FILE__, __LINE__, "error");
			return false;
		}
		if (m1->index() != m2->index()) {
      kDefaultLogger.Debugf(__FILE__, __LINE__, "error");
			return false;
		}
		if (m1->commit() != m2->commit()) {
      kDefaultLogger.Debugf(__FILE__, __LINE__, "error");
			return false;
		}
		if (m1->type() != m2->type()) {
      kDefaultLogger.Debugf(__FILE__, __LINE__, "error");
			return false;
		}
		if (m1->reject() != m2->reject()) {
      kDefaultLogger.Debugf(__FILE__, __LINE__, "error");
			return false;
		}
		if (m1->entries_size() != m2->entries_size()) {
      kDefaultLogger.Debugf(__FILE__, __LINE__, "error");
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
  Storage *s = new MemoryStorage(&kDefaultLogger);
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
  Storage *s = new MemoryStorage(&kDefaultLogger);
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
  Storage *s = new MemoryStorage(&kDefaultLogger);
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
  Storage *s = new MemoryStorage(&kDefaultLogger);
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

  Storage *s = new MemoryStorage(&kDefaultLogger);
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
		tmp(int size, map<uint64_t, bool> votes, StateType s)
			: size(size), votes(votes), state(s) {}
	};

	vector<tmp> tests;
	// win the election when receiving votes from a majority of the servers
	{
		map<uint64_t, bool>	votes;
		tests.push_back(tmp(1, votes, StateLeader));
	}
	{
		map<uint64_t, bool>	votes;
		votes[2] = true;
		votes[3] = true;
		tests.push_back(tmp(3, votes, StateLeader));
	}
	{
		map<uint64_t, bool>	votes;
		votes[2] = true;
		tests.push_back(tmp(3, votes, StateLeader));
	}
	{
		map<uint64_t, bool>	votes;
		votes[2] = true;
		votes[3] = true;
		votes[4] = true;
		votes[5] = true;
		tests.push_back(tmp(5, votes, StateLeader));
	}
	{
		map<uint64_t, bool>	votes;
		votes[2] = true;
		votes[3] = true;
		votes[4] = true;
		tests.push_back(tmp(5, votes, StateLeader));
	}
	{
		map<uint64_t, bool>	votes;
		votes[2] = true;
		votes[3] = true;
		tests.push_back(tmp(5, votes, StateLeader));
	}
	// return to follower state if it receives vote denial from a majority
	{
		map<uint64_t, bool>	votes;
		votes[2] = false;
		votes[3] = false;
		tests.push_back(tmp(3, votes, StateFollower));
	}
	{
		map<uint64_t, bool>	votes;
		votes[2] = false;
		votes[3] = false;
		votes[4] = false;
		votes[5] = false;
		tests.push_back(tmp(5, votes, StateFollower));
	}
	{
		map<uint64_t, bool>	votes;
		votes[2] = true;
		votes[3] = false;
		votes[4] = false;
		votes[5] = false;
		tests.push_back(tmp(5, votes, StateFollower));
	}
	// stay in candidate if it does not obtain the majority
	{
		map<uint64_t, bool>	votes;
		tests.push_back(tmp(3, votes, StateCandidate));
	}
	{
		map<uint64_t, bool>	votes;
		votes[2] = true;
		tests.push_back(tmp(5, votes, StateCandidate));
	}
	{
		map<uint64_t, bool>	votes;
		votes[2] = false;
		votes[3] = false;
		tests.push_back(tmp(5, votes, StateCandidate));
	}
	{
		map<uint64_t, bool>	votes;
		tests.push_back(tmp(5, votes, StateCandidate));
	}

	size_t i;
	for (i = 0; i < tests.size(); ++i) {
		tmp &t = tests[i];

		vector<uint64_t> peers;
		idsBySize(t.size, &peers);
		Storage *s = new MemoryStorage(&kDefaultLogger);
		raft *r = newTestRaft(1, peers, 10, 1, s);
		
		{
			Message msg;
			msg.set_from(1);
			msg.set_to(1);
			msg.set_type(MsgHup);
			r->step(msg);
		}
		map<uint64_t, bool>::iterator iter;
		for (iter = t.votes.begin(); iter != t.votes.end(); ++iter) {
			Message msg;
			msg.set_from(iter->first);
			msg.set_to(1);
			msg.set_type(MsgVoteResp);
			msg.set_reject(!iter->second);
			r->step(msg);
		}

		EXPECT_EQ(r->state_, t.state) << "i: " << i;
		EXPECT_EQ((int)r->term_, 1);
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
    tmp(uint64_t vote, uint64_t nvote, bool reject)
      : vote(vote), nvote(nvote), wreject(reject) {
    }
	};

	vector<tmp> tests;
  tests.push_back(tmp(kEmptyPeerId, 1, false));
  tests.push_back(tmp(kEmptyPeerId, 2, false));
  tests.push_back(tmp(1, 1, false));
  tests.push_back(tmp(2, 2, false));
  tests.push_back(tmp(1, 2, true));
  tests.push_back(tmp(2, 1, true));

  size_t i;
  for (i = 0; i < tests.size(); ++i) {
    tmp &t = tests[i];
    
    vector<uint64_t> peers;
    peers.push_back(1);
    peers.push_back(2);
    peers.push_back(3);
    Storage *s = new MemoryStorage(&kDefaultLogger);
    raft *r = newTestRaft(1, peers, 10, 1, s);

    HardState hs;
    hs.set_term(1);
    hs.set_vote(t.vote);
    r->loadState(hs);

		{
			Message msg;
			msg.set_from(t.nvote);
			msg.set_to(1);
			msg.set_type(MsgVote);
			r->step(msg);
		}

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
    
    vector<uint64_t> peers;
    peers.push_back(1);
    peers.push_back(2);
    peers.push_back(3);
    Storage *s = new MemoryStorage(&kDefaultLogger);
    raft *r = newTestRaft(1, peers, 10, 1, s);
		{
			Message tmp_msg;
			tmp_msg.set_from(1);
			tmp_msg.set_to(1);
			tmp_msg.set_type(MsgHup);
			r->step(tmp_msg);
		}
    EXPECT_EQ(r->state_, StateCandidate);
    r->step(msg);

    EXPECT_EQ(r->state_, StateFollower);
    EXPECT_EQ(r->term_, msg.term());
  }
}

// testNonleaderElectionTimeoutRandomized tests that election timeout for
// follower or candidate is randomized.
// Reference: section 5.2
void testNonleaderElectionTimeoutRandomized(StateType state) {
  uint64_t et = 10;
  
  vector<uint64_t> peers;
  peers.push_back(1);
  peers.push_back(2);
  peers.push_back(3);
  Storage *s = new MemoryStorage(&kDefaultLogger);
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
    Storage *s = new MemoryStorage(&kDefaultLogger);
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
  vector<uint64_t> peers;
  peers.push_back(1);
  peers.push_back(2);
  peers.push_back(3);
  MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
  raft *r = newTestRaft(1, peers, 10, 1, s);
  r->becomeCandidate();
  r->becomeLeader();

  commitNoopEntry(r, s);
  uint64_t li = r->raftLog_->lastIndex();
  
  {
    Entry entry;
    entry.set_data("some data");
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgProp);
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
}

// TestLeaderCommitEntry tests that when the entry has been safely replicated,
// the leader gives out the applied entries, which can be applied to its state
// machine.
// Also, the leader keeps track of the highest index it knows to be committed,
// and it includes that index in future AppendEntries RPCs so that the other
// servers eventually find out.
// Reference: section 5.3
TEST(raftPaperTests, TestLeaderCommitEntry) {
  vector<uint64_t> peers;
  peers.push_back(1);
  peers.push_back(2);
  peers.push_back(3);
  MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
  raft *r = newTestRaft(1, peers, 10, 1, s);
  r->becomeCandidate();
  r->becomeLeader();

  commitNoopEntry(r, s);
  uint64_t li = r->raftLog_->lastIndex();
  
  {
    Entry entry;
    entry.set_data("some data");
    Message msg;
    msg.set_from(1);
    msg.set_to(1);
    msg.set_type(MsgProp);
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

  r->readMessages(&msgs);

  Entry entry;
  entry.set_index(li + 1);
  entry.set_term(1);
  entry.set_data("some data");
  EntryVec g, wents;
  wents.push_back(entry);

  for (i = 0; i < msgs.size(); ++i) {
    Message *msg = msgs[i];
    EXPECT_EQ(msg->to(), i + 2);
    EXPECT_EQ(msg->type(), MsgApp);
    EXPECT_EQ(msg->commit(), li +1);
  }
}

// TestLeaderAcknowledgeCommit tests that a log entry is committed once the
// leader that created the entry has replicated it on a majority of the servers.
// Reference: section 5.3
TEST(raftPaperTests, TestLeaderAcknowledgeCommit) {
  struct tmp {
    int size;
    map<uint64_t, bool> acceptors;
    bool wack;

    tmp(int size, map<uint64_t, bool> acceptors, bool ack)
      : size(size), acceptors(acceptors), wack(ack) {}
  };

  vector<tmp> tests;
  {
    map<uint64_t, bool> acceptors;
    tests.push_back(tmp(1, acceptors, true));
  }
  {
    map<uint64_t, bool> acceptors;
    tests.push_back(tmp(3, acceptors, false));
  }
  {
    map<uint64_t, bool> acceptors;
    acceptors[2] = true;
    tests.push_back(tmp(3, acceptors, true));
  }
  {
    map<uint64_t, bool> acceptors;
    acceptors[2] = true;
    acceptors[3] = true;
    tests.push_back(tmp(3, acceptors, true));
  }
  {
    map<uint64_t, bool> acceptors;
    tests.push_back(tmp(5, acceptors, false));
  }
  {
    map<uint64_t, bool> acceptors;
    acceptors[2] = true;
    tests.push_back(tmp(5, acceptors, false));
  }
  {
    map<uint64_t, bool> acceptors;
    acceptors[2] = true;
    acceptors[3] = true;
    tests.push_back(tmp(5, acceptors, true));
  }
  {
    map<uint64_t, bool> acceptors;
    acceptors[2] = true;
    acceptors[3] = true;
    acceptors[4] = true;
    tests.push_back(tmp(5, acceptors, true));
  }
  {
    map<uint64_t, bool> acceptors;
    acceptors[2] = true;
    acceptors[3] = true;
    acceptors[4] = true;
    acceptors[5] = true;
    tests.push_back(tmp(5, acceptors, true));
  }
  size_t i;
  for (i = 0; i < tests.size(); ++i) {
    tmp &t = tests[i];
    vector<uint64_t> peers;
    idsBySize(t.size, &peers);
    MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
    raft *r = newTestRaft(1, peers, 10, 1, s);
		r->becomeCandidate();
		r->becomeLeader();
    commitNoopEntry(r, s);
    uint64_t li = r->raftLog_->lastIndex();

    {
      Entry entry;
      entry.set_data("some data");
      Message msg;
      msg.set_from(1);
      msg.set_to(1);
      msg.set_type(MsgProp);
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
  }
}

// TestLeaderCommitPrecedingEntries tests that when leader commits a log entry,
// it also commits all preceding entries in the leader’s log, including
// entries created by previous leaders.
// Also, it applies the entry to its local state machine (in log order).
// Reference: section 5.3
TEST(raftPaperTests, TestLeaderCommitPrecedingEntries) {
  vector<EntryVec> tests;
  {
    EntryVec entries; 
    tests.push_back(entries);
  }
  {
    Entry entry;
    EntryVec entries; 
    entry.set_term(2);
    entry.set_index(1);
    entries.push_back(entry);
    tests.push_back(entries);
  }
  {
    Entry entry;
    EntryVec entries; 
    entry.set_term(1);
    entry.set_index(1);
    entries.push_back(entry);

    entry.set_term(2);
    entry.set_index(2);
    entries.push_back(entry);
    tests.push_back(entries);
  }
  {
    Entry entry;
    EntryVec entries; 
    entry.set_term(1);
    entry.set_index(1);
    entries.push_back(entry);

    tests.push_back(entries);
  }
  size_t i;
  for (i = 0; i < tests.size(); ++i) {
    EntryVec &t = tests[i];
    vector<uint64_t> peers;
    peers.push_back(1);
    peers.push_back(2);
    peers.push_back(3);
    MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
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
      Message msg;
      msg.set_from(1);
      msg.set_to(1);
      msg.set_type(MsgProp);
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
    {
      Entry entry;
      entry.set_term(3);
      entry.set_index(li + 1);
      wents.push_back(entry);
    }
    {
      Entry entry;
      entry.set_term(3);
      entry.set_index(li + 2);
      entry.set_data("some data");
      wents.push_back(entry);
    }
    r->raftLog_->nextEntries(&g);
    EXPECT_TRUE(isDeepEqualEntries(g, wents)) << "i:" << i;
  }
}

// TestFollowerCommitEntry tests that once a follower learns that a log entry
// is committed, it applies the entry to its local state machine (in log order).
// Reference: section 5.3
TEST(raftPaperTests, TestFollowerCommitEntry) {
  struct tmp {
    EntryVec entries;
    uint64_t commit;
    
    tmp(EntryVec ents, uint64_t commit)
      : entries(ents), commit(commit) {
    }
  };

  vector<tmp> tests;
  {
    Entry entry;
    EntryVec entries;

    entry.set_term(1);
    entry.set_index(1);
    entry.set_data("some data");
    entries.push_back(entry);

    tests.push_back(tmp(entries, 1));
  }
  {
    Entry entry;
    EntryVec entries;

    entry.set_term(1);
    entry.set_index(1);
    entry.set_data("some data");
    entries.push_back(entry);

    entry.set_term(1);
    entry.set_index(2);
    entry.set_data("some data2");
    entries.push_back(entry);

    tests.push_back(tmp(entries, 2));
  }
  {
    Entry entry;
    EntryVec entries;

    entry.set_term(1);
    entry.set_index(1);
    entry.set_data("some data2");
    entries.push_back(entry);

    entry.set_term(1);
    entry.set_index(2);
    entry.set_data("some data");
    entries.push_back(entry);

    tests.push_back(tmp(entries, 2));
  }
  {
    Entry entry;
    EntryVec entries;

    entry.set_term(1);
    entry.set_index(1);
    entry.set_data("some data");
    entries.push_back(entry);

    entry.set_term(1);
    entry.set_index(2);
    entry.set_data("some data2");
    entries.push_back(entry);

    tests.push_back(tmp(entries, 1));
  }

  size_t i;
  for (i = 0; i < tests.size(); ++i) {
    tmp &t = tests[i];

    vector<uint64_t> peers;
    peers.push_back(1);
    peers.push_back(2);
    peers.push_back(3);
    Storage *s = new MemoryStorage(&kDefaultLogger);
    raft *r = newTestRaft(1, peers, 10, 1, s);
    r->becomeFollower(1,2);

    {
      Message msg;
      msg.set_from(2);
      msg.set_to(1);
      msg.set_term(1);
      msg.set_type(MsgApp);
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
  }
}

// TestFollowerCheckMsgApp tests that if the follower does not find an
// entry in its log with the same index and term as the one in AppendEntries RPC,
// then it refuses the new entries. Otherwise it replies that it accepts the
// append entries.
// Reference: section 5.3
TEST(raftPaperTests, TestFollowerCheckMsgApp) {
  EntryVec entries;
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

  struct tmp {
    uint64_t term;
    uint64_t index;
    uint64_t windex;
    bool wreject;
    uint64_t wrejectHint;

    tmp(uint64_t t, uint64_t i, uint64_t wi, bool wr, uint64_t wrh)
      : term(t), index(i), windex(wi), wreject(wr), wrejectHint(wrh) {}
  };

  vector<tmp> tests;

  // match with committed entries
  tests.push_back(tmp(0, 0, 1, false, 0));
  tests.push_back(tmp(entries[0].term(), entries[0].index(), 1, false, 0));
  // match with uncommitted entries
  tests.push_back(tmp(entries[1].term(), entries[1].index(), 2, false, 0));

  // unmatch with existing entry
  tests.push_back(tmp(entries[0].term(), entries[1].index(), entries[1].index(), true, 2));
  // unexisting entry
  tests.push_back(tmp(entries[1].term() + 1, entries[1].index() + 1, entries[1].index() + 1, true, 2));
  
  size_t i;
  for (i = 0; i < tests.size(); ++i) {
    tmp& t = tests[i];

    vector<uint64_t> peers;
    peers.push_back(1);
    peers.push_back(2);
    peers.push_back(3);
    MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
    EntryVec ents = entries;
    s->Append(ents);
    raft *r = newTestRaft(1, peers, 10, 1, s);
    HardState hs;
    hs.set_commit(1);
    r->loadState(hs);
    r->becomeFollower(2, 2);
    {
      Message msg;
      msg.set_type(MsgApp);
      msg.set_from(2);
      msg.set_to(1);
      msg.set_term(2);
      msg.set_term(2);
      msg.set_logterm(t.term);
      msg.set_index(t.index);

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

		tmp(uint64_t i, uint64_t t, EntryVec e, EntryVec we, EntryVec wu)
			: index(i), term(t), ents(e), wents(we), wunstable(wu) {}
	};

	vector<tmp> tests;
	{
		EntryVec ents, wents, wunstable;
		Entry entry;

		entry.set_term(3);
		entry.set_index(3);
		ents.push_back(entry);
		
		entry.set_term(1);
		entry.set_index(1);
		wents.push_back(entry);
		entry.set_term(2);
		entry.set_index(2);
		wents.push_back(entry);
		entry.set_term(3);
		entry.set_index(3);
		wents.push_back(entry);
	
		wunstable.push_back(entry);

		tests.push_back(tmp(2, 2, ents, wents, wunstable));
	}
	{
		EntryVec ents, wents, wunstable;
		Entry entry;

		entry.set_term(3);
		entry.set_index(2);
		ents.push_back(entry);
		entry.set_term(4);
		entry.set_index(3);
		ents.push_back(entry);
		
		entry.set_term(1);
		entry.set_index(1);
		wents.push_back(entry);
		entry.set_term(3);
		entry.set_index(2);
		wents.push_back(entry);
		wunstable.push_back(entry);

		entry.set_term(4);
		entry.set_index(3);
		wents.push_back(entry);
		wunstable.push_back(entry);

		tests.push_back(tmp(1, 1, ents, wents, wunstable));
	}
	{
		EntryVec ents, wents, wunstable;
		Entry entry;

		entry.set_term(1);
		entry.set_index(1);
		ents.push_back(entry);
		
		entry.set_term(1);
		entry.set_index(1);
		wents.push_back(entry);

		entry.set_term(2);
		entry.set_index(2);
		wents.push_back(entry);

		tests.push_back(tmp(0, 0, ents, wents, wunstable));
	}
	{
		EntryVec ents, wents, wunstable;
		Entry entry;

		entry.set_term(3);
		entry.set_index(1);
		ents.push_back(entry);
		wents.push_back(entry);
		wunstable.push_back(entry);

		tests.push_back(tmp(0, 0, ents, wents, wunstable));
	}

	size_t i;
	for (i = 0; i < tests.size(); ++i) {
		tmp& t = tests[i];
		vector<uint64_t> peers;
		peers.push_back(1);
		peers.push_back(2);
		peers.push_back(3);
		MemoryStorage *s = new MemoryStorage(&kDefaultLogger);

		EntryVec appEntries;
		Entry entry;
		entry.set_term(1);
		entry.set_index(1);
		appEntries.push_back(entry);
		entry.set_term(2);
		entry.set_index(2);
		appEntries.push_back(entry);

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
	}
}

// TestLeaderSyncFollowerLog tests that the leader could bring a follower's log
// into consistency with its own.
// Reference: section 5.3, figure 7
TEST(raftPaperTests, TestLeaderSyncFollowerLog) {
	EntryVec ents;
	{
		Entry entry;
		
		ents.push_back(entry);

		entry.set_term(1); entry.set_index(1); ents.push_back(entry); 
		entry.set_term(1); entry.set_index(2); ents.push_back(entry);
		entry.set_term(1); entry.set_index(3); ents.push_back(entry);

		entry.set_term(4); entry.set_index(4); ents.push_back(entry);
		entry.set_term(4); entry.set_index(5); ents.push_back(entry);

		entry.set_term(5); entry.set_index(6); ents.push_back(entry);
		entry.set_term(5); entry.set_index(7); ents.push_back(entry);

		entry.set_term(6); entry.set_index(8); ents.push_back(entry);
		entry.set_term(6); entry.set_index(9); ents.push_back(entry);
		entry.set_term(6); entry.set_index(10); ents.push_back(entry);
	}

	uint64_t term = 8;
	vector<EntryVec> tests;
	{
		Entry entry;
		EntryVec tmp_ents;
		
		tmp_ents.push_back(entry);

		entry.set_term(1); entry.set_index(1); tmp_ents.push_back(entry); 
		entry.set_term(1); entry.set_index(2); tmp_ents.push_back(entry); 
		entry.set_term(1); entry.set_index(3); tmp_ents.push_back(entry); 

		entry.set_term(4); entry.set_index(4); tmp_ents.push_back(entry);
		entry.set_term(4); entry.set_index(5); tmp_ents.push_back(entry);

		entry.set_term(5); entry.set_index(6); tmp_ents.push_back(entry);
		entry.set_term(5); entry.set_index(7); tmp_ents.push_back(entry);

		entry.set_term(6); entry.set_index(8); tmp_ents.push_back(entry);
		entry.set_term(6); entry.set_index(9); tmp_ents.push_back(entry);

		tests.push_back(tmp_ents);
	}
	{
		Entry entry;
		EntryVec tmp_ents;
		
		tmp_ents.push_back(entry);

		entry.set_term(1); entry.set_index(1); tmp_ents.push_back(entry); 
		entry.set_term(1); entry.set_index(2); tmp_ents.push_back(entry); 
		entry.set_term(1); entry.set_index(3); tmp_ents.push_back(entry); 

		entry.set_term(4); entry.set_index(4); tmp_ents.push_back(entry);

		tests.push_back(tmp_ents);
	}
	{
		Entry entry;
		EntryVec tmp_ents;
		
		tmp_ents.push_back(entry);

		entry.set_term(1); entry.set_index(1); tmp_ents.push_back(entry); 
		entry.set_term(1); entry.set_index(2); tmp_ents.push_back(entry); 
		entry.set_term(1); entry.set_index(3); tmp_ents.push_back(entry); 

		entry.set_term(4); entry.set_index(4); tmp_ents.push_back(entry);
		entry.set_term(4); entry.set_index(5); tmp_ents.push_back(entry);

		entry.set_term(5); entry.set_index(6); tmp_ents.push_back(entry);
		entry.set_term(5); entry.set_index(7); tmp_ents.push_back(entry);

		entry.set_term(6); entry.set_index(8); tmp_ents.push_back(entry);
		entry.set_term(6); entry.set_index(9); tmp_ents.push_back(entry);
		entry.set_term(6); entry.set_index(10); tmp_ents.push_back(entry);
		entry.set_term(6); entry.set_index(11); tmp_ents.push_back(entry);

		tests.push_back(tmp_ents);
	}
	{
		Entry entry;
		EntryVec tmp_ents;
		
		tmp_ents.push_back(entry);

		entry.set_term(1); entry.set_index(1); tmp_ents.push_back(entry); 
		entry.set_term(1); entry.set_index(2); tmp_ents.push_back(entry); 
		entry.set_term(1); entry.set_index(3); tmp_ents.push_back(entry); 

		entry.set_term(4); entry.set_index(4); tmp_ents.push_back(entry);
		entry.set_term(4); entry.set_index(5); tmp_ents.push_back(entry);

		entry.set_term(5); entry.set_index(6); tmp_ents.push_back(entry);
		entry.set_term(5); entry.set_index(7); tmp_ents.push_back(entry);

		entry.set_term(6); entry.set_index(8); tmp_ents.push_back(entry);
		entry.set_term(6); entry.set_index(9); tmp_ents.push_back(entry);
		entry.set_term(6); entry.set_index(10); tmp_ents.push_back(entry);

		entry.set_term(7); entry.set_index(11); tmp_ents.push_back(entry);
		entry.set_term(7); entry.set_index(12); tmp_ents.push_back(entry);

		tests.push_back(tmp_ents);
	}
	{
		Entry entry;
		EntryVec tmp_ents;
		
		tmp_ents.push_back(entry);

		entry.set_term(1); entry.set_index(1); tmp_ents.push_back(entry); 
		entry.set_term(1); entry.set_index(2); tmp_ents.push_back(entry); 
		entry.set_term(1); entry.set_index(3); tmp_ents.push_back(entry); 

		entry.set_term(4); entry.set_index(4); tmp_ents.push_back(entry);
		entry.set_term(4); entry.set_index(5); tmp_ents.push_back(entry);

		entry.set_term(4); entry.set_index(6); tmp_ents.push_back(entry);
		entry.set_term(4); entry.set_index(7); tmp_ents.push_back(entry);

		tests.push_back(tmp_ents);
	}
	{
		Entry entry;
		EntryVec tmp_ents;
		
		tmp_ents.push_back(entry);

		entry.set_term(1); entry.set_index(1); tmp_ents.push_back(entry); 
		entry.set_term(1); entry.set_index(2); tmp_ents.push_back(entry); 
		entry.set_term(1); entry.set_index(3); tmp_ents.push_back(entry); 

		entry.set_term(2); entry.set_index(4); tmp_ents.push_back(entry);
		entry.set_term(2); entry.set_index(5); tmp_ents.push_back(entry);
		entry.set_term(2); entry.set_index(6); tmp_ents.push_back(entry);

		entry.set_term(3); entry.set_index(7); tmp_ents.push_back(entry);
		entry.set_term(3); entry.set_index(8); tmp_ents.push_back(entry);
		entry.set_term(3); entry.set_index(9); tmp_ents.push_back(entry);
		entry.set_term(3); entry.set_index(10); tmp_ents.push_back(entry);
		entry.set_term(3); entry.set_index(11); tmp_ents.push_back(entry);

		tests.push_back(tmp_ents);
	}
	size_t i;
	for (i = 0; i < tests.size(); ++i) {
		EntryVec& t = tests[i];

		vector<uint64_t> peers;
		peers.push_back(1);
		peers.push_back(2);
		peers.push_back(3);

		MemoryStorage *leaderStorage = new MemoryStorage(&kDefaultLogger);
		EntryVec appEntries = ents;
    leaderStorage->Append(appEntries);
		raft *leader = newTestRaft(1, peers, 10, 1, leaderStorage);

    {
      HardState hs;
      hs.set_commit(leader->raftLog_->lastIndex());
      hs.set_term(term);
      leader->loadState(hs);
    }

		MemoryStorage *followerStorage = new MemoryStorage(&kDefaultLogger);
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
		vector<stateMachine*> sts;
		sts.push_back(new raftStateMachine(leader));
		sts.push_back(new raftStateMachine(follower));
		sts.push_back(nopStepper);
		
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
		// The election occurs in the term after the one we loaded with
		// lead.loadState above.
    {
      vector<Message> msgs;
      Message msg;
      msg.set_from(3);
      msg.set_to(1);
      msg.set_term(term + 1);
      msg.set_type(MsgVoteResp);
      msgs.push_back(msg);
      net->send(&msgs);
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

		EXPECT_EQ(raftLogString(leader->raftLog_), raftLogString(follower->raftLog_)) << "i: " << i;
	}
}

// TestVoteRequest tests that the vote request includes information about the candidate’s log
// and are sent to all of the other nodes.
// Reference: section 5.4.1
TEST(raftPaperTests, TestVoteRequest) {
  struct tmp {
    EntryVec ents;
    uint64_t wterm;

    tmp(EntryVec ents, uint64_t t)
      : ents(ents), wterm(t) {
    }
  };

  vector<tmp> tests;
  {
    EntryVec entries;
    Entry entry;
    entry.set_term(1);
    entry.set_index(1);
    entries.push_back(entry);

    tests.push_back(tmp(entries, 2));
  }
  {
    EntryVec entries;
    Entry entry;
    entry.set_term(1);
    entry.set_index(1);
    entries.push_back(entry);

    entry.set_term(2);
    entry.set_index(2);
    entries.push_back(entry);

    tests.push_back(tmp(entries, 3));
  }
  size_t i;
  for (i = 0; i < tests.size(); ++i) {
    tmp & t = tests[i];
  
    vector<uint64_t> peers;
    peers.push_back(1);
    peers.push_back(2);
    peers.push_back(3);
    Storage *s = new MemoryStorage(&kDefaultLogger);
    raft *r = newTestRaft(1, peers, 10, 1, s);
    {
      Message msg;
      msg.set_from(2);
      msg.set_to(1);
      msg.set_term(t.wterm - 1);
      msg.set_type(MsgApp);
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

    tmp(EntryVec ents, uint64_t lt, uint64_t i, bool wr)
      : ents(ents), logterm(lt), index(i), wreject(wr) {}
  };

  vector<tmp> tests;
  // same logterm
  {
    EntryVec ents;
    Entry entry;
    entry.set_term(1);
    entry.set_index(1);
    ents.push_back(entry);
    
    tests.push_back(tmp(ents, 1, 1, false));
  }
  {
    EntryVec ents;
    Entry entry;
    entry.set_term(1);
    entry.set_index(1);
    ents.push_back(entry);
    
    tests.push_back(tmp(ents, 1, 2, false));
  }
  {
    EntryVec ents;
    Entry entry;
    entry.set_term(1);
    entry.set_index(1);
    ents.push_back(entry);
    
    entry.set_term(1);
    entry.set_index(2);
    ents.push_back(entry);
    tests.push_back(tmp(ents, 1, 1, true));
  }
  // candidate higher logterm
  {
    EntryVec ents;
    Entry entry;
    entry.set_term(1);
    entry.set_index(1);
    ents.push_back(entry);
    
    tests.push_back(tmp(ents, 2, 1, false));
  }
  {
    EntryVec ents;
    Entry entry;
    entry.set_term(1);
    entry.set_index(1);
    ents.push_back(entry);
    
    tests.push_back(tmp(ents, 2, 2, false));
  }
  {
    EntryVec ents;
    Entry entry;
    entry.set_term(1);
    entry.set_index(1);
    ents.push_back(entry);
    
    entry.set_term(1);
    entry.set_index(2);
    ents.push_back(entry);
    tests.push_back(tmp(ents, 2, 1, false));
  }
  // voter higher logterm
  {
    EntryVec ents;
    Entry entry;
    entry.set_term(2);
    entry.set_index(1);
    ents.push_back(entry);
    
    tests.push_back(tmp(ents, 1, 1, true));
  }
  {
    EntryVec ents;
    Entry entry;
    entry.set_term(2);
    entry.set_index(1);
    ents.push_back(entry);
    
    tests.push_back(tmp(ents, 1, 2, true));
  }
  {
    EntryVec ents;
    Entry entry;
    entry.set_term(2);
    entry.set_index(1);
    ents.push_back(entry);
    
    entry.set_term(1);
    entry.set_index(2);
    ents.push_back(entry);
    tests.push_back(tmp(ents, 1, 1, true));
  }

  size_t i;
  for (i = 0; i < tests.size(); ++i) {
    tmp& t = tests[i];
    
    vector<uint64_t> peers;
    peers.push_back(1);
    peers.push_back(2);
    MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
    s->Append(t.ents);
    raft *r = newTestRaft(1, peers, 10, 1, s);

		{
			Message msg;
			msg.set_from(2);
			msg.set_to(1);
			msg.set_term(3);
			msg.set_logterm(t.logterm);
			msg.set_index(t.index);
			msg.set_type(MsgVote);
			r->step(msg);
		}

    MessageVec msgs;
    r->readMessages(&msgs);

    EXPECT_EQ((int)msgs.size(), 1);
    Message *msg = msgs[0];
    
    EXPECT_EQ(msg->type(), MsgVoteResp);
    EXPECT_EQ(msg->reject(), t.wreject);
  }
}

// TestLeaderOnlyCommitsLogFromCurrentTerm tests that only log entries from the leader’s
// current term are committed by counting replicas.
// Reference: section 5.4.2
TEST(raftPaperTests, TestLeaderOnlyCommitsLogFromCurrentTerm) {
  EntryVec entries; 
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
  struct tmp {
    uint64_t index, wcommit;
    tmp(uint64_t i, uint64_t w) 
      : index(i), wcommit(w){}
  };

  vector<tmp> tests;
  // do not commit log entries in previous terms
  tests.push_back(tmp(1, 0));
  tests.push_back(tmp(2, 0));
  // commit log in current term
  tests.push_back(tmp(3, 3));

  size_t i;
  for (i = 0; i < tests.size(); ++i) {
    tmp& t = tests[i];
    EntryVec ents = entries;

		MemoryStorage *s = new MemoryStorage(&kDefaultLogger);
    s->Append(ents);
		vector<uint64_t> peers;
		peers.push_back(1);
		peers.push_back(2);
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
      Message msg;
      msg.set_from(1);
      msg.set_to(1);
      msg.set_type(MsgProp);
      msg.add_entries();
      r->step(msg);
    }
    {
      Entry entry;
      Message msg;
      msg.set_from(2);
      msg.set_to(1);
      msg.set_term(r->term_);
      msg.set_index(t.index);
      msg.set_type(MsgAppResp);
      r->step(msg);
    }

    EXPECT_EQ(r->raftLog_->committed_, t.wcommit);
  }
}
