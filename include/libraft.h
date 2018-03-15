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
  virtual int InitialState(HardState *, ConfState *) = 0;
  virtual int FirstIndex(uint64_t *index) = 0;
  virtual int LastIndex(uint64_t *index) = 0;
  virtual int Term(uint64_t i, uint64_t *term) = 0;
  virtual int Entries(uint64_t lo, uint64_t hi, uint64_t maxSize, vector<Entry> *entries) = 0;
  virtual int Snapshot(Snapshot **snapshot) = 0;
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

struct Config {
  uint64_t          id;
  vector<uint64_t>  peers;
  int               electionTick;
  int               heartbeatTick;
  Storage*          storage;
  uint64_t          applied;
  uint64_t          maxSizePerMsg;
  uint64_t          maxInflightMsgs;
  Logger*           logger;            
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
