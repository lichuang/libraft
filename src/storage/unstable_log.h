#ifndef __UNSTABLE_LOG_H__
#define __UNSTABLE_LOG_H__

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
  Logger *logger_;

  void truncateAndAppend(const EntryVec& entries);

  bool maybeFirstIndex(uint64_t *first);

  bool  maybeLastIndex(uint64_t* last);

  bool maybeTerm(uint64_t i, uint64_t *term);

  void stableTo(uint64_t i, uint64_t t);

  void stableSnapTo(uint64_t i);

  void restore(const Snapshot& snapshot);

  void slice(uint64_t lo, uint64_t hi, EntryVec *entries);

  void mustCheckOutOfBounds(uint64_t lo, uint64_t hi);
};
}; // namespace libraft

#endif  // __UNSTABLE_LOG_H__
