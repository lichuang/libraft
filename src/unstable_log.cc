#include "unstable_log.h"

uint64_t unstableLog::maybeFirstIndex() {
  if (snapshot_ != NULL) {
    return snapshot_->metadata().index() + 1
  }

  return 0;
}

uint64_t unstableLog::maybeLastIndex() {
  if (entries_.size() > 0) {
    return offset_ + entries_.size() - 1;
  }

  if (snapshot_ != NULL) {
    return snapshot_->metadata().index();
  }

  return 0;
}

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

  last = maybeLastIndex();
  if (!last) {
    return 0;
  }

  if (i > last) {
    return 0;
  }
  return entries_[i - offset_].term();
}

void unstableLog::stableTo(uint64_t i, uint64_t t) {
  gt = maybeTerm(i);
  if (!gt) {
    return;
  }

  if (gt == t && i >= offset_) {
    entries_.erase(entries_.begin(), entries_.begin() + i + 1 - offset_);
    offset_ = i + 1;
  }
}

void unstableLog::stableSnapTo(uint64_t i) {
  if (snapshot_ != NULL && snapshot_.metadata().index() == i) {
    snapshot_ = NULL;
  }
}

void unstableLog::restore(Snapshot *snapshot) {
  offset_ = snapshot->metadata().index() + 1;
  entries_.clear();
  snapshot_ = snapshot;
}

void unstableLog::truncateAndAppend(const vector<Entry> entries) {
  uint64_t after = entries[0].index();

  if (after == offset_ + uint64_t(entries_.size())) {
    entries_ += entries;
    return;
  }

  if (after < offset_) {
    logger_->Infof("replace the unstable entries from index %d", after);
    offset_ = after;
    entries_ = entries;
    return;
  }

  logger_->Infof("truncate the unstable entries before index %d", after);
  vector<Entry> slice;
  slice(offset, after, &slice);
  entries_ = slice;
  entries_ += entries;
}

void unstableLog::slice(uint64_t lo, uint64_t hi, vector<Entry> *entries) {
  mustCheckOutOfBounds(lo, hi);
  entries->assign(entries_->begin() + lo - offset_, entries_->begin() + hi - offset_);
}

void unstableLog::mustCheckOutOfBounds(uint64_t lo, uint64_t hi) {
  if (lo > hi) {
    log_->Fatalf("invalid unstable.slice %d > %d", lo, hi);
  }

  upper = offset_ + (uint64_t)entries_.size();
  if lo < offset_ || upper < hi {
    log_->Fatalf("unstable.slice[%d,%d) out of bound [%d,%d]", lo, hi, u.offset, upper)
  }
}
