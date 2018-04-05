#include <gtest/gtest.h>
#include <math.h>
#include "libraft.h"
#include "util.h"
#include "raft.h"
#include "memory_storage.h"
#include "default_logger.h"
#include "progress.h"
#include "raft_test_util.h"
#include "read_only.h"

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

bool isDeepEqualMsgs(const vector<Message*>& msgs1, const vector<Message*>& msgs2) {
	if (msgs1.size() != msgs2.size()) {
		return false;
	}
	int i;
	for (i = 0; i < msgs1.size(); ++i) {
		Message *m1 = msgs1[i];
		Message *m2 = msgs2[i];
		if (m1->from() != m2->from()) {
			return false;
		}
		if (m1->to() != m2->to()) {
			return false;
		}
		if (m1->term() != m2->term()) {
			return false;
		}
		if (m1->logterm() != m2->logterm()) {
			return false;
		}
		if (m1->index() != m2->index()) {
			return false;
		}
		if (m1->commit() != m2->commit()) {
			return false;
		}
		if (m1->type() != m2->type()) {
			return false;
		}
		if (m1->reject() != m2->reject()) {
			return false;
		}
	}
	return true;
}

// testUpdateTermFromMessage tests that if one server’s current term is
// smaller than the other’s, then it updates its current term to the larger
// value. If a candidate or leader discovers that its term is out of date,
// it immediately reverts to follower state.
// Reference: section 5.1
void testUpdateTermFromMessage(StateType state) {
  vector<uint64_t> peers;
  peers.push_back(1);
  peers.push_back(2);
  peers.push_back(3);
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
	}

  {
    Message msg;
    msg.set_type(MsgApp);
    msg.set_term(2);

    r->step(msg);
  }

	EXPECT_EQ(r->term_, 2);
	EXPECT_EQ(r->state_, StateFollower);
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
TEST(raftPaperTests, TestRejectStaleTermMessage) {
}

// TestStartAsFollower tests that when servers start up, they begin as followers.
// Reference: section 5.2
TEST(raftPaperTests, TestStartAsFollower) {
  vector<uint64_t> peers;
  peers.push_back(1);
  peers.push_back(2);
  peers.push_back(3);
  Storage *s = new MemoryStorage(&kDefaultLogger);
  raft *r = newTestRaft(1, peers, 10, 1, s);
	EXPECT_EQ(r->state_, StateFollower);
}

// TestLeaderBcastBeat tests that if the leader receives a heartbeat tick,
// it will send a msgApp with m.Index = 0, m.LogTerm=0 and empty entries as
// heartbeat to all followers.
// Reference: section 5.2
TEST(raftPaperTests, TestLeaderBcastBeat) {
	// heartbeat interval
	uint64_t hi = 1;
  vector<uint64_t> peers;
  peers.push_back(1);
  peers.push_back(2);
  peers.push_back(3);
  Storage *s = new MemoryStorage(&kDefaultLogger);
  raft *r = newTestRaft(1, peers, 10, 1, s);
	r->becomeCandidate();
	r->becomeLeader();

  EntryVec entries;
  int i;
  for (i = 0; i < 10; ++i) {
    Entry entry;
    entry.set_index(i + 1);
    entries.push_back(entry);
  }
  r->appendEntry(&entries);

  for (i = 0; i < hi; ++i) {
		r->tick();
	}
  vector<Message*> msgs;
  r->readMessages(&msgs);

	vector<Message*> wmsgs;
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
  vector<uint64_t> peers;
  peers.push_back(1);
  peers.push_back(2);
  peers.push_back(3);
  Storage *s = new MemoryStorage(&kDefaultLogger);
  raft *r = newTestRaft(1, peers, 10, 1, s);

	switch (state) {
	case StateFollower:
		r->becomeFollower(1,2);
		break;
	case StateCandidate:
		r->becomeCandidate();
		break;
	}
	int i;
  for (i = 0; i < 2*et; ++i) {
		r->tick();
	}
	EXPECT_EQ(r->term_, 2);
	EXPECT_EQ(r->state_, StateCandidate);
	EXPECT_TRUE(r->votes_[r->id_]);
  vector<Message*> msgs;
  r->readMessages(&msgs);

	vector<Message*> wmsgs;
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

	int i;
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
		EXPECT_EQ(r->term_, 1);
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
  tests.push_back(tmp(None, 1, false));
  tests.push_back(tmp(None, 2, false));
  tests.push_back(tmp(1, 1, false));
  tests.push_back(tmp(2, 2, false));
  tests.push_back(tmp(1, 2, true));
  tests.push_back(tmp(2, 1, true));

  int i;
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

    vector<Message*> msgs;
    r->readMessages(&msgs);

    vector<Message*> wmsgs;
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
  int i;
  for (i = 0; i < tests.size(); ++i) {
    Message& msg = tests[i];
    
    vector<uint64_t> peers;
    peers.push_back(1);
    peers.push_back(2);
    peers.push_back(3);
    Storage *s = new MemoryStorage(&kDefaultLogger);
    raft *r = newTestRaft(1, peers, 10, 1, s);
		{
			Message msg;
			msg.set_from(1);
			msg.set_to(1);
			msg.set_type(MsgHup);
			r->step(msg);
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
  int i;
  map<int, bool> timeouts;
  for (i = 0; i < 50 * et; ++i) {
    switch (state) {
    case StateFollower:
      r->becomeFollower(r->term_ + 1,2);
      break;
    case StateCandidate:
      r->becomeCandidate();
      break;
    }

    uint64_t time = 0;
    vector<Message*> msgs;
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
  int i;
  for (i = 0; i < peers.size(); ++i) {
    Storage *s = new MemoryStorage(&kDefaultLogger);
    raft *r = newTestRaft(peers[i], peers, et, 1, s);
    rs.push_back(r);
  }
  int conflicts = 0;
  for (i = 0; i < 1000; ++i) {
    int j;
    for (j = 0; j < rs.size(); ++j) {
      raft *r = rs[j];
      switch (state) {
      case StateFollower:
        r->becomeFollower(r->term_ + 1,None);
        break;
      case StateCandidate:
        r->becomeCandidate();
        break;
      }
    }

    int timeoutNum = 0;
    while (timeoutNum == 0) {
      for (j = 0; j < rs.size(); ++j) {
        raft *r = rs[j];
        r->tick();
        vector<Message*> msgs;
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
