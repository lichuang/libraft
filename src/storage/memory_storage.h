/*
 * Copyright (C) lichuang
 */

#ifndef __LIBRAFT_MEMORY_STORAGE_H__
#define __LIBRAFT_MEMORY_STORAGE_H__

#include "libraft.h"
#include "base/mutex.h"

namespace libraft {

// MemoryStorage implements the Storage interface backed by an
// in-memory array.
class MemoryStorage : public Storage {
public:
  MemoryStorage(Logger *logger, EntryVec* entries = NULL);
  virtual ~MemoryStorage();

  int InitialState(HardState *, ConfState *);
  int FirstIndex(uint64_t *index);
  int LastIndex(uint64_t *index);
  int Term(uint64_t i, uint64_t *term);
  int Entries(uint64_t lo, uint64_t hi, uint64_t maxSize, EntryVec *entries);
  int GetSnapshot(Snapshot **snapshot);
  int SetHardState(const HardState& );

  int Append(const EntryVec& entries);
  int Compact(uint64_t compactIndex);
  int ApplySnapshot(const Snapshot& snapshot);
  int CreateSnapshot(uint64_t i, ConfState *cs, const string& data, Snapshot *ss);

private:
  uint64_t firstIndex();
  uint64_t lastIndex();

public:
  HardState hardState_;
  Snapshot  *snapShot_;
  
  // ents[i] has raft log position i+snapshot.Metadata.Index
  EntryVec entries_;

	// Protects access to all fields. Most methods of MemoryStorage are
	// run on the raft goroutine, but Append() is run on an application
	// goroutine.
  Locker locker_;

  Logger *logger_;
};

}; // namespace libraft

#endif  // __LIBRAFT_MEMORY_STORAGE_H__
