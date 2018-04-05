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
    kDefaultLogger.Debugf(__FILE__, __LINE__, "error");
		return false;
	}
	int i;
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

void commitNoopEntry(raft *r, Storage *s) {
  EXPECT_EQ(r->state_, StateLeader);
  r->bcastAppend();
  // simulate the response of MsgApp
  vector<Message*> msgs;
  r->readMessages(&msgs);

  int i;
  for (i = 0; i < msgs.size(); ++i) {
    Message *msg = msgs[i];
    EXPECT_FALSE(msg->type() != MsgApp || msg->entries_size() != 1 || msg->entries(0).has_data());
    r->step(acceptAndReply(msg));
  }
  // ignore further messages to refresh followers' commit index
  r->readMessages(&msgs);
  EntryVec entries;
  r->raftLog_->unstableEntries(&entries);
  s->Append(&entries);
  r->raftLog_->appliedTo(r->raftLog_->committed_);
  r->raftLog_->stableTo(r->raftLog_->lastIndex(), r->raftLog_->lastTerm());
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
  Storage *s = new MemoryStorage(&kDefaultLogger);
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

  vector<Message*> msgs, wmsgs;
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
  Storage *s = new MemoryStorage(&kDefaultLogger);
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

  vector<Message*> msgs;
  r->readMessages(&msgs);
  int i;
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
  int i;
  for (i = 0; i < tests.size(); ++i) {
    tmp &t = tests[i];
    vector<uint64_t> peers;
    idsBySize(t.size, &peers);
    Storage *s = new MemoryStorage(&kDefaultLogger);
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
    vector<Message*> msgs;
    r->readMessages(&msgs);
    int j;
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
  int i;
  for (i = 0; i < tests.size(); ++i) {
    EntryVec &t = tests[i];
    vector<uint64_t> peers;
    peers.push_back(1);
    peers.push_back(2);
    peers.push_back(3);
    Storage *s = new MemoryStorage(&kDefaultLogger);
    EntryVec appEntries = t;
    s->Append(&appEntries);
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

    vector<Message*> msgs;
    r->readMessages(&msgs);
    int j;
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

  int i;
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
      int j;
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
TEST(raftPaperTests, TestFollowerCommitEntry) {
}
