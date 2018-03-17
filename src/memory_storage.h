#ifndef __MEMORY_STORAGE_H__
#define __MEMORY_STORAGE_H__

#include "libraft.h"
#include "mutex.h"

class MemoryStorage : public Storage {
public:
  MemoryStorage(Logger *logger);
  virtual ~MemoryStorage();

  int InitialState(HardState *, ConfState *);
  int FirstIndex(uint64_t *index);
  int LastIndex(uint64_t *index);
  int Term(uint64_t i, uint64_t *term);
  int Entries(uint64_t lo, uint64_t hi, uint64_t maxSize, vector<Entry> *entries);
  int GetSnapshot(Snapshot **snapshot);
  int SetHardState(const HardState& );

  int Append(EntryVec* entries);
  int Compact(uint64_t compactIndex);
  int ApplySnapshot(const Snapshot& snapshot);
private:
  uint64_t firstIndex();
  uint64_t lastIndex();

public:
  HardState hardState_;
  Snapshot  *snapShot_;
  // ents[i] has raft log position i+snapshot.Metadata.Index
  EntryVec entries_;

  Locker locker_;
  Logger *logger_;
};

#endif  // __MEMORY_STORAGE_H__
