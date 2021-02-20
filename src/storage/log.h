/*
 * Copyright (C) lichuang
 */

#ifndef __LIBRAFT_LOG_H__
#define __LIBRAFT_LOG_H__

#include "libraft.h"
#include "unstable_log.h"

namespace libraft {

// Raft log storage
struct raftLog {
  // storage contains all stable entries since the last snapshot.
  Storage *storage_;

  // unstable contains all unstable entries and snapshot.
  // they will be saved into storage.
  unstableLog unstable_;

  // committed is the highest log position that is known to be in
  // stable storage on a quorum of nodes.
  uint64_t committed_;

  // applied is the highest log position that the application has
  // been instructed to apply to its state machine.
  // Invariant: applied <= committed
  uint64_t applied_;

  Logger *logger_;

  raftLog(Storage *, Logger *);
  string String();

  // maybeAppend returns false if the entries cannot be appended. Otherwise,
  // it returns last index of new entries.
  bool maybeAppend(uint64_t index, uint64_t logTerm, 
                   uint64_t committed, const EntryVec& entries, uint64_t *lasti);

  // append entries to unstable storage and return last index
  uint64_t append(const EntryVec& entries);

  // finds the index of the conflict.
  uint64_t findConflict(const EntryVec& entries);

  // get all unstable entries
  void unstableEntries(EntryVec *entries);

  // nextEntries returns all the available entries for execution.
  void nextEntries(EntryVec* entries);

  // hasNextEntries returns if there is any available entries for execution.
  bool hasNextEntries();

  // return snapshot of raft log
  int snapshot(Snapshot **snapshot);

  uint64_t firstIndex();

  uint64_t lastIndex();

  // change committed_ index to tocommit
  void commitTo(uint64_t tocommit);

  // change applied index to i
  void appliedTo(uint64_t i);

  // unstable storage stable index to log
  void stableTo(uint64_t i, uint64_t t);

  // unstable storage stable index to snapshot
  void stableSnapTo(uint64_t i);

  // get last index term
  uint64_t lastTerm();

  // get entries from index i, no more than maxSize
  int entries(uint64_t i, uint64_t maxSize, EntryVec *entries);

  // allEntries returns all entries in the log.
  void allEntries(EntryVec *entries);

  // isUpToDate determines if the given (lastIndex,term) log is more up-to-date
  // by comparing the index and term of the last entries in the existing logs.
  bool isUpToDate(uint64_t lasti, uint64_t term);

  // return true if the term of the index equal to term
  bool matchTerm(uint64_t i, uint64_t term);

  // return true if maxIndex committed
  bool maybeCommit(uint64_t maxIndex, uint64_t term);

  // restore from snapshot
  void restore(const Snapshot& snapshot);

  // slice returns a slice of log entries from lo through hi-1, inclusive.
  int slice(uint64_t lo, uint64_t hi, uint64_t maxSize, EntryVec* entries);

  // check if [lo,hi] is out of bounds
  int mustCheckOutOfBounds(uint64_t lo, uint64_t hi);

  // returns the term of the entry at index i, if there is any.
  int term(uint64_t i, uint64_t *t);

  uint64_t zeroTermOnErrCompacted(uint64_t t, int err);
};

// newLog returns log using the given storage. It recovers the log to the state
// that it just commits and applies the latest snapshot.
extern raftLog* newLog(Storage *storage, Logger *logger);

}; // namespace libraft

#endif // __LIBRAFT_LOG_H__
