/*
 * Copyright (C) lichuang
 */

#ifndef __LIBRAFT_RAFT_H__
#define __LIBRAFT_RAFT_H__

#include <map>
#include "libraft.h"
#include "core/progress.h"
#include "storage/log.h"

using namespace std;

namespace libraft {

struct readOnly;
struct ReadState;

enum CampaignType {
  // CampaignPreElection represents the first phase of a normal election when
  // Config.PreVote is true.
  CampaignPreElection = 0,
  // CampaignElection represents a normal (time-based) election (the second phase
  // of the election when Config.PreVote is true).
  CampaignElection = 1,
  // CampaignTransfer represents the type of leader transfer
  CampaignTransfer = 2
};

struct raft;

typedef void (*stepFun)(raft *, const Message&);

// the Raft State Machine
struct raft {
  uint64_t id_;
  uint64_t term_;
  uint64_t vote_;

  vector<ReadState*> readStates_;
  raftLog *raftLog_;
  int maxInfilght_;
  uint64_t maxMsgSize_;

  // cluster node Progress Map
  map<uint64_t, Progress*> progressMap_;
  
  StateType state_;
  map<uint64_t, bool> votes_;

  // save every out msg in outMsgs_,then msgs will be moved to `Ready' struct
  MessageVec outMsgs_;

  // current leader id, default is kNone.
  uint64_t leader_;

  // leadTransferee is id of the leader transfer target when its value is not zero.
  // Follow the procedure defined in raft thesis 3.10.
  // default is kNone.
  uint64_t leadTransferee_;

  // New configuration is ignored if there exists unapplied configuration.
  bool pendingConf_;
  readOnly* readOnly_;

  // number of ticks since it reached last electionTimeout when it is leader
  // or candidate.
  // number of ticks since it reached last electionTimeout or received a
  // valid message from current leader when it is a follower.
  int electionElapsed_;

  // number of ticks since it reached last heartbeatTimeout.
  // only leader keeps heartbeatElapsed.
  int heartbeatElapsed_;

  // number of ticks timeout to send heartbeat
  int heartbeatTimeout_;

  // number of ticks timeout to election
  int electionTimeout_;

  bool checkQuorum_;
  bool preVote_;

  // randomizedElectionTimeout is a random number between
  // [electiontimeout, 2 * electiontimeout - 1]. It gets reset
  // when raft changes its state to follower or candidate.
  int randomizedElectionTimeout_;

  Logger* logger_;

  // current role state machine function
  stepFun stateStepFunc_;

  raft(const Config *, raftLog *);

  // called by Node in each `Tick'
  void tick();

  // load HardState in hs
  void loadState(const HardState &hs);

  // return current cluster node id in nodes
  void nodes(vector<uint64_t> *nodes);

  // return true if leader_ is not none
  bool hasLeader();

  // return SoftState in ss
  void softState(SoftState *ss);

  // return HardState in hs
  void hardState(HardState *hs);

  // return current cluster quorum
  int quorum();

  // send out messages
  void send(Message *msg);

  // send append message
  void sendAppend(uint64_t to);

  // send heartbeat message
  void sendHeartbeat(uint64_t to, const string &ctx);

  // broadcast append message to cluster
  void bcastAppend();

  // broadcast heartbeat message to cluster
  void bcastHeartbeat();
  void bcastHeartbeatWithCtx(const string &ctx);

  // change to follower state
  void becomeFollower(uint64_t term, uint64_t leader);

  // change to candidate state
  void becomeCandidate();

  // change to pre-candidate state
  void becomePreCandidate();

  // change to leader state
  void becomeLeader();

  void campaign(CampaignType t);

  // maybeCommit attempts to advance the commit index. Returns true if
  // the commit index changed (in which case the caller should 
  // call r.bcastAppend).  
  bool maybeCommit();

  // reset to term
  void reset(uint64_t term);

  // append entries to storage
  void appendEntry(EntryVec* entries);

  void handleAppendEntries(const Message& msg);
  void handleHeartbeat(const Message& msg);
  void handleSnapshot(const Message& msg);

  // tickElection is run by followers and candidates after r.electionTimeout.
  void tickElection();

  // tickHeartbeat is run by leaders to send a MsgBeat after r.heartbeatTimeout.
  void tickHeartbeat();

  // v means peer `id' accepted or not,after it return num of 
  // granted peers in cluster
  int  poll(uint64_t id, MessageType t, bool v);

  int  step(const Message& msg);

  // promotable indicates whether state machine can be promoted to leader,
  // which is true when its own id is in progress list.  
  bool promotable();

  bool restore(const Snapshot& snapshot);
  void delProgress(uint64_t id);
  void addNode(uint64_t id);
  void removeNode(uint64_t id);

  // pastElectionTimeout returns true if electionElapsed_ >= randomizedElectionTimeout_
  bool pastElectionTimeout();

  // reset randomizedElectionTimeout_ to [electiontimeout, 2 * electiontimeout - 1]
  void resetRandomizedElectionTimeout();

  void setProgress(uint64_t id, uint64_t match, uint64_t next);
  void abortLeaderTransfer();
  void proxyMessage(const Message& msg);

  // read out messages,after call will clean the outMsgs_(only used in test)
  void readMessages(MessageVec *);

  // checkQuorumActive returns true if the quorum is active from
  // the view of the local raft state machine.  
  // checkQuorumActive also resets all RecentActive to false.
  bool checkQuorumActive();

  // send timeout msg 
  void sendTimeoutNow(uint64_t to);

  void resetPendingConf();
};

extern raft* newRaft(const Config *);
string entryString(const Entry& entry);

// different role's state machine functions
void stepLeader(raft *r, const Message& msg);
void stepCandidate(raft* r, const Message& msg);
void stepFollower(raft* r, const Message& msg);

}; // namespace libraft

#endif  // __LIBRAFT_RAFT_H__
