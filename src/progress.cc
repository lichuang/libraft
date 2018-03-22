#include "progress.h"

Progress::Progress(uint64_t next, int maxInfilght, Logger *logger)
  : match_(0),
    next_(next),
    state_(ProgressStateProbe),
    paused_(false),  
    pendingSnapshot_(0),
    recentActive_(false),
    ins_(inflights(maxInfilght, logger)),
    logger_(logger) {
}

Progress::~Progress() {
}

void Progress::resetState(ProgressState state) {
  paused_ = false;
  pendingSnapshot_ = 0;
  state_ = state;
  ins_.reset();
}

void Progress::becomeProbe() {
  // If the original state is ProgressStateSnapshot, progress knows that
  // the pending snapshot has been sent to this peer successfully, then
  // probes from pendingSnapshot + 1.
  if (state_ == ProgressStateSnapshot) {
    uint64_t pendingSnapshot = pendingSnapshot_;
    resetState(ProgressStateProbe);
    next_ = max(match_ + 1, pendingSnapshot + 1);
  } else {
    resetState(ProgressStateProbe);
    next_ = match_ + 1;
  }
}

void Progress::becomeReplicate() {
  resetState(ProgressStateReplicate);
  next_ = match_ + 1;
}

void Progress::becomeSnapshot(uint64_t snapshoti) {
  resetState(ProgressStateSnapshot);
  pendingSnapshot_ = snapshoti;
}

// maybeUpdate returns false if the given n index comes from an outdated message.
// Otherwise it updates the progress and returns true.
bool Progress::maybeUpdate(uint64_t n) {
  bool updated = false;
  if (match_ < n) {
    match_ = n;
    updated = true;
    resume();
  }
  if (next_ < n + 1) {
    next_ = n + 1;
  }
  return updated;
}

void Progress::optimisticUpdate(uint64_t n) {
  next_ = n + 1;
}

void Progress::snapshotFailure() {
  pendingSnapshot_ = 0;
}

// maybeDecrTo returns false if the given to index comes from an out of order message.
// Otherwise it decreases the progress next index to min(rejected, last) and returns true.
bool Progress::maybeDecrTo(uint64_t rejected, uint64_t last) {
  if (state_ == ProgressStateReplicate) {
    // the rejection must be stale if the progress has matched and "rejected"
    // is smaller than "match".
    if (rejected <= match_) {
      return false;
    }
    // directly decrease next to match + 1
    next_ = match_ + 1;
    return true;
  }

  // the rejection must be stale if "rejected" does not match next - 1
  if (next_ - 1 != rejected) {
    return false;
  }

  next_ = min(rejected, last + 1);
  if (next_ < 1) {
    next_ = 1;
  }
  resume();
  return true;
}

void Progress::pause() {
  paused_ = true;
}

void Progress::resume() {
  paused_ = false;
}

const char* Progress::stateString() {
  if (state_ == ProgressStateProbe) {
    return "ProgressStateProbe";
  }
  if (state_ == ProgressStateSnapshot) {
    return "ProgressStateSnapshot";
  }
  if (state_ == ProgressStateReplicate) {
    return "ProgressStateReplicate";
  }

  return "unknown state";
}

// IsPaused returns whether sending log entries to this node has been
// paused. A node may be paused because it has rejected recent
// MsgApps, is currently waiting for a snapshot, or has reached the
// MaxInflightMsgs limit.
bool Progress::isPaused() {
  switch (state_) {
  case ProgressStateProbe:
    return paused_;
  case ProgressStateReplicate:
    return ins_.full();
  case ProgressStateSnapshot:
    return true;
  }
}

// needSnapshotAbort returns true if snapshot progress's Match
// is equal or higher than the pendingSnapshot.
bool Progress::needSnapshotAbort() {
  return state_ == ProgressStateSnapshot && match_ >= pendingSnapshot_;
}

string Progress::string() {
  char tmp[500];
  snprintf(tmp, sizeof(tmp), "next = %llu, match = %llu, state = %s, waiting = %d, pendingSnapshot = %llu",
    next_, match_, stateString(), isPaused(), pendingSnapshot_);
  return std::string(tmp);
}

void inflights::add(uint64_t infight) {
  if (full()) {
    logger_->Fatalf(__FILE__, __LINE__, "cannot add into a full inflights");
  }

  uint64_t next = start_ + count_;
  uint64_t size = size_;

  if (next >= size) {
    next -= size;
  }

  if (next >= buffer_.size()) {
    growBuf();
  }
  buffer_[next] = infight;
  count_++;
}

// grow the inflight buffer by doubling up to inflights.size. We grow on demand
// instead of preallocating to inflights.size to handle systems which have
// thousands of Raft groups per process.
void inflights::growBuf() {
  uint64_t newSize = buffer_.size() * 2;
  if (newSize == 0) {
    newSize = 1;
  } else if (newSize > size_) {
    newSize = size_;
  }

  buffer_.resize(newSize);
}

// freeTo frees the inflights smaller or equal to the given `to` flight.
void inflights::freeTo(uint64_t to) {
  if (count_ == 0 || to < buffer_[start_]) {
    return;
  }

  uint64_t i = 0, idx = start_;
  for (i = 0; i < count_; ++i) {
    if (to < buffer_[idx]) {  // found the first large inflight
      break;
    }

    // increase index and maybe rotate
    uint64_t size = size_;
    ++idx;
    if (idx >= size) {
      idx -= size;
    }
  }

  // free i inflights and set new start index
  count_ -= i;
  start_  = idx;
  if (count_ == 0) {
    // inflights is empty, reset the start index so that we don't grow the
    // buffer unnecessarily.
    start_ = 0;
  }
}

void inflights::freeFirstOne() {
  freeTo(buffer_[start_]);
}

bool inflights::full() {
  return count_ == size_;
}

void inflights::reset() {
  count_ = 0;
  start_ = 0;
}
