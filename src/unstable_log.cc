#include "unstable_log.h"

// maybeFirstIndex returns the index of the first possible entry in entries
// if it has a snapshot.
uint64_t unstableLog::maybeFirstIndex() {
  if (snapshot_ != NULL) {
    return snapshot_->metadata().index() + 1;
  }

  return 0;
}

// maybeLastIndex returns the last index if it has at least one
// unstable entry or snapshot.
uint64_t unstableLog::maybeLastIndex() {
  if (entries_.size() > 0) {
    return offset_ + entries_.size() - 1;
  }

  if (snapshot_ != NULL) {
    return snapshot_->metadata().index();
  }

  return 0;
}

// maybeTerm returns the term of the entry at index i, if there
// is any.
uint64_t unstableLog::maybeTerm(uint64_t i) {
  if (i < offset_) {
    if (snapshot_ == NULL) {
      return 0;
    }
    if (snapshot_->metadata().index() == i) {
      return snapshot_->metadata().term();
    }

    return 0;
  }

  uint64_t last = maybeLastIndex();
  if (last == 0) {
    return 0;
  }
  if (i > last) {
    return 0;
  }
  return entries_[i - offset_].term();
}

void unstableLog::stableTo(uint64_t i, uint64_t t) {
  uint64_t gt = maybeTerm(i);
  if (!gt) {
    return;
  }

  if (gt == t && i >= offset_) {
    entries_.erase(entries_.begin(), entries_.begin() + i + 1 - offset_);
    offset_ = i + 1;
  }
}

void unstableLog::stableSnapTo(uint64_t i) {
  if (snapshot_ != NULL && snapshot_->metadata().index() == i) {
    delete snapshot_;
    snapshot_ = NULL;
  }
}

void unstableLog::restore(Snapshot *snapshot) {
  offset_ = snapshot->metadata().index() + 1;
  entries_.clear();
  snapshot_ = snapshot;
}

void unstableLog::truncateAndAppend(const EntryVec& entries) {
  uint64_t after = entries[0].index();

  if (after == offset_ + uint64_t(entries_.size())) {
    // after is the next index in the u.entries
    // directly append
    entries_.insert(entries_.end(), entries.begin(), entries.end());
    return;
  }

  if (after <= offset_) {
    // The log is being truncated to before our current offset
    // portion, so set the offset and replace the entries
    logger_->Infof(__FILE__, __LINE__, "replace the unstable entries from index %llu", after);
    offset_ = after;
    entries_ = entries;
    return;
  }

  // truncate to after and copy to u.entries then append
  logger_->Infof(__FILE__, __LINE__, "truncate the unstable entries before index %llu", after);
  vector<Entry> slice;
  this->slice(offset_, after, &slice);
  entries_ = slice;
  entries_.insert(entries_.end(), entries.begin(), entries.end());
}

void unstableLog::slice(uint64_t lo, uint64_t hi, EntryVec *entries) {
  mustCheckOutOfBounds(lo, hi);
  entries->assign(entries_.begin() + lo - offset_, entries_.begin() + hi - offset_);
}

// u.offset <= lo <= hi <= u.offset+len(u.offset)
void unstableLog::mustCheckOutOfBounds(uint64_t lo, uint64_t hi) {
  if (lo > hi) {
    logger_->Fatalf(__FILE__, __LINE__, "invalid unstable.slice %llu > %llu", lo, hi);
  }

  uint64_t upper = offset_ + (uint64_t)entries_.size();
  if (lo < offset_ || upper < hi) {
    logger_->Fatalf(__FILE__, __LINE__, "unstable.slice[%llu,%llu) out of bound [%llu,%llu]", lo, hi, offset_, upper);
  }
}
