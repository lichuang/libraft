/*
 * Copyright (C) lichuang
 */

#ifndef __LIB_RAFT_H__
#define __LIB_RAFT_H__

#include <cstdint>
#include <climits>
#include <string>
#include <vector>
#include "proto/raft.pb.h"

using namespace std;
using namespace raftpb;

namespace libraft {

const static uint64_t kEmptyPeerId = 0;
const static uint64_t kNoLimit = ULONG_MAX;

enum ErrorCode {
  OK                                = 0,

  // ErrCompacted is returned by Storage.Entries/Compact when a requested
  // index is unavailable because it predates the last snapshot.  
  ErrCompacted                      = 1,

  // ErrSnapOutOfDate is returned by Storage.CreateSnapshot when a requested
  // index is older than the existing snapshot.  
  ErrSnapOutOfDate                  = 2,

  // ErrUnavailable is returned by Storage interface when the requested log entries
  // are unavailable.  
  ErrUnavailable                    = 3,

  // ErrSnapshotTemporarilyUnavailable is returned by the Storage interface when the required
  // snapshot is temporarily unavailable.  
  ErrSnapshotTemporarilyUnavailable = 4,

  // ErrSerializeFail is returned by the Node interface when the request data Serialize failed. 
  ErrSerializeFail                  = 5,

  // Number of error code
  NumErrorCode
};

static const char* 
kErrString[NumErrorCode] = {
  "OK",
  "ErrCompacted",
  "ErrSnapOutOfDate",
  "ErrUnavailable",
  "ErrSnapshotTemporarilyUnavailable",
  "ErrSerializeFail",
};

inline const char* 
GetErrorString(int err) {
  return kErrString[err];
}

inline bool SUCCESS(int err) { return err == OK; }

enum StateType {
  StateFollower = 0,
  StateCandidate = 1,
  StateLeader = 2,
  StatePreCandidate = 3,
  NumStateType
};

struct SoftState {
  uint64_t leader;
  StateType state;

  SoftState()
    : leader(kEmptyPeerId)
    , state(StateFollower) {}

  inline SoftState& operator=(const SoftState& from) {
    leader = from.leader;
    state  = from.state;
    return *this;
  }
};

// ReadState provides state for read only query.
// It's caller's responsibility to call ReadIndex first before getting
// this state from ready, It's also caller's duty to differentiate if this
// state is what it requests through RequestCtx, eg. given a unique id as
// RequestCtx
struct ReadState {
  uint64_t index;
  string   requestCtx;
  ReadState(uint64_t i, const string &ctx)
    : index(i),
      requestCtx(ctx) {}
};

typedef vector<Entry> EntryVec;
typedef vector<Message*> MessageVec;

struct Ready {
 	// The current volatile state of a Node.
	// SoftState will be nil if there is no update.
	// It is not required to consume or store SoftState.
  SoftState         softState;

	// The current state of a Node to be saved to stable storage BEFORE
	// Messages are sent.
	// HardState will be equal to empty state if there is no update.
  HardState         hardState;

 	// ReadStates can be used for node to serve linearizable read requests locally
	// when its applied index is greater than the index in ReadState.
	// Note that the readState will be returned when raft receives msgReadIndex.
	// The returned is only valid for the request that requested to read.
  vector<ReadState*> readStates;

	// Entries specifies entries to be saved to stable storage BEFORE
	// Messages are sent.
  EntryVec          entries;

  // Snapshot specifies the snapshot to be saved to stable storage.
  Snapshot          *snapshot;

	// CommittedEntries specifies entries to be committed to a
	// store/state-machine. These have previously been committed to stable
	// store.
  EntryVec          committedEntries;

 	// Messages specifies outbound messages to be sent AFTER Entries are
	// committed to stable storage.
	// If it contains a MsgSnap message, the application MUST report back to raft
	// when the snapshot has been received or has failed by calling ReportSnapshot.
  MessageVec  messages;
};

// Storage is an interface that may be implemented by the application
// to retrieve log entries from storage.
//
// If any Storage method returns an error, the raft instance will
// become inoperable and refuse to participate in elections; the
// application is responsible for cleanup and recovery in this case.
class Storage {
public:
  virtual ~Storage() {}

  // InitialState returns the saved HardState and ConfState information.
  virtual int InitialState(HardState *, ConfState *) = 0;

	// FirstIndex returns the index of the first log entry that is
	// possibly available via Entries (older entries have been incorporated
	// into the latest Snapshot; if storage only contains the dummy entry the
	// first log entry is not available).
  virtual int FirstIndex(uint64_t *index) = 0;

	// LastIndex returns the index of the last entry in the log.  
  virtual int LastIndex(uint64_t *index) = 0;

	// Term returns the term of entry i, which must be in the range
	// [FirstIndex()-1, LastIndex()]. The term of the entry before
	// FirstIndex is retained for matching purposes even though the
	// rest of that entry may not be available.  
  virtual int Term(uint64_t i, uint64_t *term) = 0;

	// Entries returns a slice of log entries in the range [lo,hi).
	// MaxSize limits the total size of the log entries returned, but
	// Entries returns at least one entry if any.  
  virtual int Entries(uint64_t lo, uint64_t hi, uint64_t maxSize, EntryVec *entries) = 0;

	// Snapshot returns the most recent snapshot.
	// If snapshot is temporarily unavailable, it should return ErrSnapshotTemporarilyUnavailable,
	// so raft state machine could know that Storage needs some time to prepare
	// snapshot and call Snapshot later.
  virtual int GetSnapshot(Snapshot **snapshot) = 0;

  //int SetHardState(const HardState& );
  //virtual int Append(const EntryVec& entries) = 0;
  //virtual int CreateSnapshot(uint64_t i, ConfState *cs, const string& data, Snapshot *ss) = 0;
};

// ReadOnlyOption specifies how the read only request is processed.
enum ReadOnlyOption {
  // ReadOnlySafe guarantees the linearizability of the read only request by
  // communicating with the quorum. It is the default and suggested option.
  ReadOnlySafe = 0,

  // ReadOnlyLeaseBased ensures linearizability of the read only request by
  // relying on the leader lease. It can be affected by clock drift.
  // If the clock drift is unbounded, leader might keep the lease longer than it
  // should (clock can move backward/pause without any bound). ReadIndex is not safe
  // in that case.
  ReadOnlyLeaseBased = 1
};

typedef void (*raft_log_func)(const char * buf);

enum LogLevel {
  Debug     = 0,
  Warn      = 1,
  Info      = 2,
  Error     = 3,
  Fatal     = 4,
};

// Config contains the parameters to start a raft.
struct Config {
  // ID is the identity of the local raft. ID cannot be 0.
  uint64_t          id = kEmptyPeerId;

  // peers contains the IDs of all nodes (including self) in the raft cluster. It
  // should only be set when starting a new raft cluster. Restarting raft from
  // previous configuration will panic if peers is set. peer is private and only
  // used for testing right now.
  vector<uint64_t>  peers;

  // electionTick is the number of Node.Tick invocations that must pass between
  // elections. That is, if a follower does not receive any message from the
  // leader of current term before ElectionTick has elapsed, it will become
  // candidate and start an election. ElectionTick must be greater than
  // HeartbeatTick. We suggest ElectionTick = 10 * HeartbeatTick to avoid
  // unnecessary leader switching.
  int               electionTick = 10;

  // heartbeatTick is the number of Node.Tick invocations that must pass between
  // heartbeats. That is, a leader sends heartbeat messages to maintain its
  // leadership every HeartbeatTick ticks.
  int               heartbeatTick = 1;

  // storage is the storage for raft. raft generates entries and states to be
  // stored in storage. raft reads the persisted entries and states out of
  // Storage when it needs. raft reads out the previous state and configuration
  // out of storage when restarting.
  // when node end up, storage will be destroyed.
  // if it is NULL, use `MemoryStorage' by default.
  Storage*          storage = NULL;

  // applied is the last applied index. It should only be set when restarting
  // raft. raft will not return entries to the application smaller or equal to
  // Applied. If Applied is unset when restarting, raft might return previous
  // applied entries. This is a very application dependent configuration.
  uint64_t          applied = 0;

  // maxSizePerMsg limits the max size of each append message. Smaller value
  // lowers the raft recovery cost(initial probing and message lost during normal
  // operation). On the other side, it might affect the throughput during normal
  // replication. Note: math.MaxUint64 for unlimited, 0 for at most one entry per
  // message.
  uint64_t          maxSizePerMsg = 1024 * 1024;

  // maxInflightMsgs limits the max number of in-flight append messages during
  // optimistic replication phase. The application transportation layer usually
  // has its own sending buffer over TCP/UDP. Setting MaxInflightMsgs to avoid
  // overflowing that sending buffer. TODO (xiangli): feedback to application to
  // limit the proposal rate?
  uint64_t          maxInflightMsgs = 1024;

  // checkQuorum specifies if the leader should check quorum activity. Leader
  // steps down when quorum is not active for an electionTimeout.
  bool checkQuorum = false;

  // preVote enables the Pre-Vote algorithm described in raft thesis section
  // 9.6. This prevents disruption when a node that has been partitioned away
  // rejoins the cluster.
  bool preVote = false;

  // log level of raft log, Debug by default
  LogLevel logLevel = Debug;

  // logFunc is the logger function used for raft log. For multinode which can host
  // multiple raft group, each raft group can have its own logger.
  // when node end up, storage will be destroyed.
  // if it is NULL, the default logger will send log to stdout.
  raft_log_func logFunc = NULL;

  ReadOnlyOption    readOnlyOption;
};

struct Peer {
  uint64_t Id;
  string   Context;
};

class Node {
public:
	// Tick increments the internal logical clock for the Node by a single tick. Election
	// timeouts and heartbeat timeouts are in units of ticks.
  virtual void Tick(Ready **ready) = 0;

  // Campaign causes the Node to transition to candidate state and start campaigning to become leader.
  virtual int Campaign(Ready **ready) = 0;

  // Propose proposes that data be appended to the log.
  virtual int Propose(const string& data, Ready **ready) = 0;

	// ProposeConfChange proposes config change.
	// At most one ConfChange can be in the process of going through consensus.
	// Application needs to call ApplyConfChange when applying EntryConfChange type entry.
  virtual int ProposeConfChange(const ConfChange& cc, Ready **ready) = 0;

  // Step advances the state machine using the given message. ctx.Err() will be returned, if any.
  virtual int Step(const Message& msg, Ready **ready) = 0;

 	// Advance notifies the Node that the application has saved progress up to the last Ready.
	// It prepares the node to return the next available Ready.
	//
	// The application should generally call Advance after it applies the entries in last Ready.
	//
	// However, as an optimization, the application may call Advance while it is applying the
	// commands. For example. when the last Ready contains a snapshot, the application might take
	// a long time to apply the snapshot data. To continue receiving Ready without blocking raft
	// progress, it can call Advance before finishing applying the last ready.
  virtual void Advance() = 0;

	// ApplyConfChange applies config change to the local node.
	// Returns an opaque ConfState protobuf which must be recorded
	// in snapshots. Will never return nil; it returns a pointer only
	// to match MemoryStorage.Compact.
  virtual void ApplyConfChange(const ConfChange& cc, ConfState *cs, Ready **ready) = 0;

  // TransferLeadership attempts to transfer leadership to the given transferee.
  virtual void TransferLeadership(uint64_t leader, uint64_t transferee, Ready **ready) = 0;

	// ReadIndex request a read state. The read state will be set in the ready.
	// Read state has a read index. Once the application advances further than the read
	// index, any linearizable read requests issued before the read request can be
	// processed safely. The read state will have the same rctx attached.
  virtual int ReadIndex(const string &rctx, Ready **ready) = 0;

	// Stop performs any necessary termination of the Node.
  virtual void Stop() = 0;
};

extern Node* StartNode(Config *config, const vector<Peer>& peers);
extern Node* RestartNode(Config *config);

// empty (hard,soft) state constants
static const HardState kEmptyHardState;
static const SoftState kEmptySoftState;

}; // namespace libraft

#endif  // __LIB_RAFT_H__
