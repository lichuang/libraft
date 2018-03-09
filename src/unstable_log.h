#ifndef __UNSTABLE_LOG_H__
#define __UNSTABLE_LOG_H__

#include "libraft.h"

struct unstableLog {
  Snapshot *snapshot_;

  vector<Entry> entries_;
  uint64_t offset_;
  Logger *logger_;

  // return the index of the first possible entry in entries
  // if it has a snapshot
  // return 0 otherwise
  uint64_t maybeFirstIndex();

  // maybeLastIndex returns the last index if it has at least one
  // unstable entry or snapshot.
  uint64_t maybeLastIndex();

  // maybeTerm returns the term of the entry at index i, if there
  // is any.
  uint64_t maybeTerm(uint64_t i);

  void stableTo(uint64_t i, uint64_t t);

  void stableSnapTo(uint64_t i);

  void restore(Snapshot *snapshot);

  void truncateAndAppend(const vector<Entry> entries);

  void slice(uint64_t lo, uint64_t hi, vector<Entry> *entries);

  void mustCheckOutOfBounds(uint64_t lo, uint64_t hi);
};

#endif  // __UNSTABLE_LOG_H__
