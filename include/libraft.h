#ifndef __LIB_RAFT_H__
#define __LIB_RAFT_H__

#include <cstdint>
#include <climits>
#include <string>
#include <vector>
#include "../src/raft.pb.h"

using namespace std;
using namespace raftpb;

const static uint64_t None = 0;
const static uint64_t noLimit = ULONG_MAX;

enum ErrorCode {
  OK                                = 0,
  ErrCompacted                      = 1,
  ErrSnapOutOfDate                  = 2,
  ErrUnavailable                    = 3,
  ErrSnapshotTemporarilyUnavailable = 4
};

inline bool SUCCESS(int err) { return err == OK; }

enum StateType {
  StateFollower = 0,
  StateCandidate = 1,
  StateLeader = 2,
  StatePreCandidate = 3
};

struct SoftState {
  uint64_t leader;
  StateType state;
};

// ReadState provides state for read only query.
// It's caller's responsibility to call ReadIndex first before getting
// this state from ready, It's also caller's duty to differentiate if this
// state is what it requests through RequestCtx, eg. given a unique id as
// RequestCtx
struct ReadState {
  uint64_t index_;
  string   requestCtx_;
  ReadState(uint64_t index, const string &ctx)
    : index_(index),
      requestCtx_(ctx) {}
};

typedef vector<Entry> EntryVec;

struct Ready {
  SoftState         softState;
  HardState         hardState;
  vector<ReadState> readStates;
  vector<Entry>     entries;
  vector<Entry>     committedEntries;
  vector<Message>   messages;
};

class Storage {
public:
  virtual ~Storage() {}

  virtual int InitialState(HardState *, ConfState *) = 0;
  virtual int FirstIndex(uint64_t *index) = 0;
  virtual int LastIndex(uint64_t *index) = 0;
  virtual int Term(uint64_t i, uint64_t *term) = 0;
  virtual int Entries(uint64_t lo, uint64_t hi, uint64_t maxSize, vector<Entry> *entries) = 0;
  virtual int GetSnapshot(Snapshot **snapshot) = 0;
  virtual int SetHardState(const HardState& ) = 0;
  virtual int Append(EntryVec* entries) = 0;
};

class Logger {
public:
  virtual void Debugf(const char *file, int line, const char *fmt, ...) = 0;
  virtual void Infof(const char *file, int line, const char *fmt, ...) = 0;
  virtual void Warningf(const char *file, int line, const char *fmt, ...) = 0;
  virtual void Errorf(const char *file, int line, const char *fmt, ...) = 0;
  virtual void Fatalf(const char *file, int line, const char *fmt, ...) = 0;
};

enum ReadOnlyOption {
  // ReadOnlySafe guarantees the linearizability of the read only request by
  // communicating with the quorum. It is the default and suggested option.
  ReadOnlySafe = 1,
  // ReadOnlyLeaseBased ensures linearizability of the read only request by
  // relying on the leader lease. It can be affected by clock drift.
  // If the clock drift is unbounded, leader might keep the lease longer than it
  // should (clock can move backward/pause without any bound). ReadIndex is not safe
  // in that case.
  ReadOnlyLeaseBased = 2
};

// Config contains the parameters to start a raft.
struct Config {
  // ID is the identity of the local raft. ID cannot be 0.
  uint64_t          id;

  // peers contains the IDs of all nodes (including self) in the raft cluster. It
  // should only be set when starting a new raft cluster. Restarting raft from
  // previous configuration will panic if peers is set. peer is private and only
  // used for testing right now.
  vector<uint64_t>  peers;

  // ElectionTick is the number of Node.Tick invocations that must pass between
  // elections. That is, if a follower does not receive any message from the
  // leader of current term before ElectionTick has elapsed, it will become
  // candidate and start an election. ElectionTick must be greater than
  // HeartbeatTick. We suggest ElectionTick = 10 * HeartbeatTick to avoid
  // unnecessary leader switching.
  int               electionTick;

  // HeartbeatTick is the number of Node.Tick invocations that must pass between
  // heartbeats. That is, a leader sends heartbeat messages to maintain its
  // leadership every HeartbeatTick ticks.
  int               heartbeatTick;

  // Storage is the storage for raft. raft generates entries and states to be
  // stored in storage. raft reads the persisted entries and states out of
  // Storage when it needs. raft reads out the previous state and configuration
  // out of storage when restarting.
  Storage*          storage;

  // Applied is the last applied index. It should only be set when restarting
  // raft. raft will not return entries to the application smaller or equal to
  // Applied. If Applied is unset when restarting, raft might return previous
  // applied entries. This is a very application dependent configuration.
  uint64_t          applied;

  // MaxSizePerMsg limits the max size of each append message. Smaller value
  // lowers the raft recovery cost(initial probing and message lost during normal
  // operation). On the other side, it might affect the throughput during normal
  // replication. Note: math.MaxUint64 for unlimited, 0 for at most one entry per
  // message.
  uint64_t          maxSizePerMsg;

  // MaxInflightMsgs limits the max number of in-flight append messages during
  // optimistic replication phase. The application transportation layer usually
  // has its own sending buffer over TCP/UDP. Setting MaxInflightMsgs to avoid
  // overflowing that sending buffer. TODO (xiangli): feedback to application to
  // limit the proposal rate?
  uint64_t          maxInflightMsgs;

  // CheckQuorum specifies if the leader should check quorum activity. Leader
  // steps down when quorum is not active for an electionTimeout.
  bool checkQuorum;

  // PreVote enables the Pre-Vote algorithm described in raft thesis section
  // 9.6. This prevents disruption when a node that has been partitioned away
  // rejoins the cluster.
  bool preVote;

  // Logger is the logger used for raft log. For multinode which can host
  // multiple raft group, each raft group can have its own logger
  Logger*           logger;            

  // ReadOnlyOption specifies how the read only request is processed.
  //
  // ReadOnlySafe guarantees the linearizability of the read only request by
  // communicating with the quorum. It is the default and suggested option.
  //
  // ReadOnlyLeaseBased ensures linearizability of the read only request by
  // relying on the leader lease. It can be affected by clock drift.
  // If the clock drift is unbounded, leader might keep the lease longer than it
  // should (clock can move backward/pause without any bound). ReadIndex is not safe
  // in that case.
  ReadOnlyOption    readOnlyOption;
};

class Node {
public:
  /*
  virtual void Stop() = 0;
  virtual int Campaign(string data) = 0;
  virtual int Propose(string data) = 0;
  */
  virtual void Tick(Ready **ready) = 0;
};

extern Node* StartNode(Config *config);
extern Node* RestartNode(Config *config);
extern const char* GetErrorString(int err);

#endif  // __LIB_RAFT_H__
