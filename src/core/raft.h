#ifndef __RAFT_H__
#define __RAFT_H__

#include <map>
#include "libraft.h"
#include "storage/log.h"
#include "core/progress.h"

using namespace std;
namespace libraft {
struct readOnly;
struct ReadState;

enum CampaignType {
  // campaignPreElection represents the first phase of a normal election when
  // Config.PreVote is true.
  campaignPreElection = 1,
  // campaignElection represents a normal (time-based) election (the second phase
  // of the election when Config.PreVote is true).
  campaignElection = 2,
  // campaignTransfer represents the type of leader transfer
  campaignTransfer = 3
};

struct raft;

typedef void (*stepFun)(raft *, const Message&);

struct raft {
  uint64_t id_;
  uint64_t term_;
  uint64_t vote_;

  vector<ReadState*> readStates_;
  raftLog *raftLog_;
  int maxInfilght_;
  uint64_t maxMsgSize_;
  map<uint64_t, Progress*> prs_;
  StateType state_;
  map<uint64_t, bool> votes_;
  MessageVec msgs_;

  uint64_t leader_;
  // leadTransferee is id of the leader transfer target when its value is not zero.
  // Follow the procedure defined in raft thesis 3.10.
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
  int heartbeatTimeout_;
  int electionTimeout_;

  bool checkQuorum_;
  bool preVote_;

  // randomizedElectionTimeout is a random number between
  // [electiontimeout, 2 * electiontimeout - 1]. It gets reset
  // when raft changes its state to follower or candidate.
  int randomizedElectionTimeout_;

  Logger* logger_;

  stepFun stateStep;

  raft(const Config *, raftLog *);
  void tick();
  const char* getCampaignString(CampaignType t);
  void loadState(const HardState &hs);
  void nodes(vector<uint64_t> *nodes);
  bool hasLeader();
  void softState(SoftState *ss);
  void hardState(HardState *hs);
  int quorum();
  void send(Message *msg);

  void sendAppend(uint64_t to);
  void sendHeartbeat(uint64_t to, const string &ctx);
  void bcastAppend();
  void bcastHeartbeat();
  void bcastHeartbeatWithCtx(const string &ctx);
  void becomeFollower(uint64_t term, uint64_t leader);
  void becomeCandidate();
  void becomePreCandidate();
  void becomeLeader();
  void campaign(CampaignType t);
  bool maybeCommit();
  void reset(uint64_t term);
  void appendEntry(EntryVec* entries);
  void handleAppendEntries(const Message& msg);
  void handleHeartbeat(const Message& msg);
  void handleSnapshot(const Message& msg);
  void tickElection();
  void tickHeartbeat();
  int  poll(uint64_t id, MessageType t, bool v);
  int  step(const Message& msg);
  bool promotable();
  bool restore(const Snapshot& snapshot);
  void delProgress(uint64_t id);
  void addNode(uint64_t id);
  void removeNode(uint64_t id);
  bool pastElectionTimeout();
  void resetRandomizedElectionTimeout();
  void setProgress(uint64_t id, uint64_t match, uint64_t next);
  void abortLeaderTransfer();
  void proxyMessage(const Message& msg);
  void readMessages(MessageVec *);
  bool checkQuorumActive();
  void sendTimeoutNow(uint64_t to);
  void resetPendingConf();
};

extern raft* newRaft(const Config *);
string entryString(const Entry& entry);

void stepLeader(raft *r, const Message& msg);
void stepCandidate(raft* r, const Message& msg);
void stepFollower(raft* r, const Message& msg);
}; // namespace libraft

#endif  // __RAFT_H__
