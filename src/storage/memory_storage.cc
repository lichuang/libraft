/*
 * Copyright (C) lichuang
 */

#include "base/logger.h"
#include "base/util.h"
#include "storage/memory_storage.h"

namespace libraft {

MemoryStorage::MemoryStorage(EntryVec* entries) 
  : snapShot_(new Snapshot()) {
  if (entries == NULL) {
    // When starting from scratch populate the list with a dummy entry at term zero.
    entries_.push_back(Entry());
  } else {        
    for (size_t i = 0; i < entries->size(); ++i) {
      entries_.push_back((*entries)[i]);
    }
  }
}

MemoryStorage::~MemoryStorage() {
  delete snapShot_;
}

int
MemoryStorage::InitialState(HardState *hs, ConfState *cs) {
  *hs = hardState_;
  *cs = snapShot_->metadata().conf_state();
  return OK;
}

int
MemoryStorage::SetHardState(const HardState& hs) {
  Mutex mutex(&locker_);
  hardState_ = hs;
  return OK;
}

uint64_t
MemoryStorage::firstIndex() {
  return entries_[0].index() + 1;
}

int
MemoryStorage::FirstIndex(uint64_t *index) {
  Mutex mutex(&locker_);
  *index = firstIndex();
  return OK;
}

int
MemoryStorage::LastIndex(uint64_t *index) {
  Mutex mutex(&locker_);
  *index = lastIndex();
  return OK;
}

uint64_t
MemoryStorage::lastIndex() {
  return entries_[0].index() + entries_.size() - 1;
}

int
MemoryStorage::Term(uint64_t i, uint64_t *term) {
  Mutex mutex(&locker_);
  *term = 0;
  uint64_t offset = entries_[0].index();
  if (i < offset) {
    return ErrCompacted;
  }
  if (i - offset >= entries_.size()) {
    return ErrUnavailable;
  }
  *term = entries_[i - offset].term();
  return OK;
}

int
MemoryStorage::Entries(uint64_t lo, uint64_t hi, uint64_t maxSize, EntryVec *entries) {
  Mutex mutex(&locker_);

  // first check validity of index
  uint64_t offset = entries_[0].index();
  if (lo <= offset) {
    return ErrCompacted;
  }
  if (hi > lastIndex() + 1) {
    return ErrUnavailable;
  }
  
  // only contains dummy entries.
  if (entries_.size() == 1) {
    return ErrUnavailable;
  }
  size_t i;
  for (i = lo - offset; i < hi - offset; ++i) {
    entries->push_back(entries_[i]);
  }
  limitSize(maxSize, entries);
  return OK;
}

int
MemoryStorage::GetSnapshot(Snapshot **snapshot) {
  Mutex mutex(&locker_);
  *snapshot = snapShot_;
  return OK;
}

// Compact discards all log entries prior to compactIndex.
// It is the application's responsibility to not attempt to compact an index
// greater than raftLog.applied.
int
MemoryStorage::Compact(uint64_t compactIndex) {
  Mutex mutex(&locker_);

  uint64_t offset = entries_[0].index();
  if (compactIndex <= offset) {
    return ErrCompacted;
  }
  if (compactIndex > lastIndex()) {
    Fatalf("compact %llu is out of bound lastindex(%llu)", compactIndex, lastIndex());
  }

  uint64_t i = compactIndex - offset;
  EntryVec entries;
  Entry entry;
  entry.set_index(entries_[i].index());
  entry.set_term(entries_[i].term());
  entries.push_back(entry);
  for (i = i + 1; i < entries_.size(); ++i) {
    entries.push_back(entries_[i]);
  }
  entries_ = entries;
  return OK;
}

// ApplySnapshot overwrites the contents of this Storage object with
// those of the given snapshot.
int
MemoryStorage::ApplySnapshot(const Snapshot& snapshot) {
  Mutex mutex(&locker_);

  //handle check for old snapshot being applied
  uint64_t index = snapShot_->metadata().index();
  uint64_t snapIndex = snapshot.metadata().index();
  if (index >= snapIndex) {
    return ErrSnapOutOfDate;
  }

  snapShot_->CopyFrom(snapshot);
  entries_.clear();
  Entry entry;
  entry.set_index(snapshot.metadata().index());
  entry.set_term(snapshot.metadata().term());
  entries_.push_back(entry);
  return OK;
}

// Append the new entries to storage.
// entries[0].Index > ms.entries[0].Index
int
MemoryStorage::Append(const EntryVec& entries) {
  if (entries.empty()) {
    return OK;
  }

  Mutex mutex(&locker_);
  size_t i;  

  uint64_t first = firstIndex();
  uint64_t last  = entries[0].index() + entries.size() - 1;

  // shortcut if there is no new entry.
  if (last < first) {
    return OK;
  }

  EntryVec appendEntries;
  // truncate compacted entries
  if (first > entries[0].index()) {
    uint64_t index = first - entries[0].index();
    appendEntries = EntryVec(entries.begin() + index, entries.end());
  } else {
    appendEntries = entries;
  }

  uint64_t offset = appendEntries[0].index() - entries_[0].index();
  // TODO: optimize vector copy
  if (entries_.size() > offset) {
    EntryVec tmp_ents;
    //tmp_ents.push_back(Entry());
    for (i = 0; i < offset; ++i) {
      tmp_ents.push_back(entries_[i]);
    }
    entries_.clear();
    entries_ = tmp_ents;
    for (i = 0; i < appendEntries.size(); ++i) {
      entries_.push_back(appendEntries[i]);
    }
    return OK;
  }

  if (entries_.size() == offset) {
    for (i = 0; i < appendEntries.size(); ++i) {
      entries_.push_back(appendEntries[i]);
    }
    return OK;
  }

  Fatalf("missing log entry [last: %llu, append at: %llu]",
    lastIndex(), appendEntries[0].index());
  return OK;
}

// CreateSnapshot makes a snapshot which can be retrieved with Snapshot() and
// can be used to reconstruct the state at that point.
// If any configuration changes have been made since the last compaction,
// the result of the last ApplyConfChange must be passed in.
int
MemoryStorage::CreateSnapshot(uint64_t i, ConfState *cs, const string& data, Snapshot *ss) {
  Mutex mutex(&locker_);

  if (i <= snapShot_->metadata().index()) {
    return ErrSnapOutOfDate;
  }

  uint64_t offset = entries_[0].index();
  if (i > lastIndex()) {
    Fatalf("snapshot %d is out of bound lastindex(%llu)", i, lastIndex());
  }

  snapShot_->mutable_metadata()->set_index(i);  
  snapShot_->mutable_metadata()->set_term(entries_[i - offset].term());  
  if (cs != NULL) {
    *(snapShot_->mutable_metadata()->mutable_conf_state()) = *cs;
  }
  snapShot_->set_data(data);
  if (ss != NULL) {
    *ss = *snapShot_;
  }

  return OK;
}

}; // namespace libraft