#include "memory_storage.h"
#include "util.h"

MemoryStorage::MemoryStorage() : snapShot_(new Snapshot()) {
  // When starting from scratch populate the list with a dummy entry at term zero.
  entries_.push_back(Entry());
}

MemoryStorage::~MemoryStorage() {
  delete snapShot_;
}

int MemoryStorage::InitialState(HardState *hs, ConfState *cs) {
  *hs = hardState_;
  *cs = snapShot_->metadata().conf_state();
  return OK;
}

int MemoryStorage::SetHardState(const HardState& hs) {
  Mutex mutex(&locker_);
  hardState_ = hs;
  return OK;
}

uint64_t MemoryStorage::firstIndex() {
  return entries_[0].index() + 1;
}

int MemoryStorage::FirstIndex(uint64_t *index) {
  Mutex mutex(&locker_);
  *index = firstIndex();
  return OK;
}

int MemoryStorage::LastIndex(uint64_t *index) {
  Mutex mutex(&locker_);
  *index = lastIndex();
  return OK;
}

uint64_t MemoryStorage::lastIndex() {
  return entries_[0].index() + entries_.size() - 1;
}

int MemoryStorage::Term(uint64_t i, uint64_t *term) {
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

int MemoryStorage::Entries(uint64_t lo, uint64_t hi, uint64_t maxSize, vector<Entry> *entries) {
  Mutex mutex(&locker_);
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
  int i;
  for (i = lo - offset; i < hi - offset; ++i) {
    entries->push_back(entries_[i]);
  }
  limitSize(maxSize, entries);
  return OK;
}

int MemoryStorage::GetSnapshot(Snapshot **snapshot) {
  Mutex mutex(&locker_);
  *snapshot = snapShot_;
  return OK;
}
