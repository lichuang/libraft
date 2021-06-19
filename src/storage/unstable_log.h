/*
 * Copyright (C) lichuang
 */

#ifndef __LIBRAFT_UNSTABLE_LOG_H__
#define __LIBRAFT_UNSTABLE_LOG_H__

#include "libraft.h"

namespace libraft {

// unstable.entries[i] has raft log position i+unstable.offset.
// Note that unstable.offset may be less than the highest log
// position in storage; this means that the next write to storage
// might need to truncate the log before persisting unstable.entries.
struct unstableLog {
  unstableLog() : snapshot_(NULL) {
  }

  // the incoming unstable snapshot, if any.
  Snapshot* snapshot_;

  // all entries that have not yet been written to storage.
  EntryVec entries_;
  uint64_t offset_;

  void truncateAndAppend(const EntryVec& entries);

  // maybeFirstIndex returns the index of the first possible entry in entries
  // if it has a snapshot.
  bool maybeFirstIndex(uint64_t *first);

  // maybeLastIndex returns the last index if it has at least one
  // unstable entry or snapshot.
  bool maybeLastIndex(uint64_t* last);

  bool maybeTerm(uint64_t i, uint64_t *term);

  void stableTo(uint64_t i, uint64_t t);

  void stableSnapTo(uint64_t i);

  void restore(const Snapshot& snapshot);

  void slice(uint64_t lo, uint64_t hi, EntryVec *entries);

  void mustCheckOutOfBounds(uint64_t lo, uint64_t hi);
};

}; // namespace libraft

#endif  // __LIBRAFT_UNSTABLE_LOG_H__
