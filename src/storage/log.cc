/*
 * Copyright (C) lichuang
 */

#include "base/logger.h"
#include "base/util.h"
#include "storage/log.h"

namespace libraft {

raftLog::raftLog(Storage *storage) 
  : storage_(storage),
    committed_(0),
    applied_(0) {
}

raftLog::~raftLog() {
  delete storage_;
}

// maybeAppend returns false if the entries cannot be appended. Otherwise,
// it returns last index of new entries.
bool
raftLog::maybeAppend(uint64_t index, uint64_t logTerm, 
                     uint64_t committed, const EntryVec& entries, uint64_t *lasti) {
  *lasti = 0;

  // check if log index and term match
  if (!matchTerm(index, logTerm)) {
    return false;
  }

  uint64_t lastNewI, ci, offset;

  lastNewI = index + (uint64_t)entries.size();

  // check if there is conflict entries
  ci = findConflict(entries);
  
  // if conflict entries is already committed
  if (ci != 0 && ci <= committed_) {
    Fatalf("entry %llu conflict with committed entry [committed(%llu)]", ci, committed_);
  }

  if (ci != 0) {
    offset = index + 1;
    EntryVec appendEntries(entries.begin() + ci - offset, entries.end());
    append(appendEntries);
  }

  commitTo(min(committed, lastNewI));
  *lasti = lastNewI;

  return true;
}

void
raftLog::commitTo(uint64_t tocommit) {
  // never decrease commit
  if (committed_ >= tocommit) {
    return;
  }

  // to commit index cannot bigger than last index
  if (lastIndex() < tocommit) {
    Fatalf( 
      "tocommit(%llu) is out of range [lastIndex(%llu)]. Was the raft log corrupted, truncated, or lost?",
      tocommit, lastIndex());
  }

  committed_ = tocommit;
  Debugf("commit to %llu", committed_);
}

void
raftLog::appliedTo(uint64_t i) {
  if (i == 0) {
    return;
  }

  // applied index cannot bigger than committed index,
  // also cannot smaller than already applied index.
  if (committed_ < i || i < applied_) {
    Fatalf("applied(%llu) is out of range [prevApplied(%llu), committed(%llu)]", i, applied_, committed_);
  }
  applied_ = i;
}

void
raftLog::stableTo(uint64_t i, uint64_t t) {
  unstable_.stableTo(i, t);
}

void
raftLog::stableSnapTo(uint64_t i) {
  unstable_.stableSnapTo(i);
}

uint64_t
raftLog::lastTerm() {
  int err;
  uint64_t t;

  err = term(lastIndex(), &t);
  if (!SUCCESS(err)) {
    Fatalf("unexpected error when getting the last term (%s)", kErrString[err]);
  }

  return t;
}

int
raftLog::entries(uint64_t i, uint64_t maxSize, EntryVec *entries) {
  entries->clear();
  uint64_t lasti = lastIndex();

  // valid index check
  if (i > lasti) {
    return OK;
  }

  return slice(i, lasti + 1, maxSize, entries);
}

// allEntries returns all entries in the log.
void
raftLog::allEntries(EntryVec *entries) {
  int err = this->entries(firstIndex(), kNoLimit, entries);
  if (SUCCESS(err)) {
    return;
  }

  if (err == ErrCompacted) { // try again if there was a racing compaction
    return allEntries(entries);
  }
  Fatalf("allEntries fatal: %s", kErrString[err]);
}

// isUpToDate determines if the given (lastIndex,term) log is more up-to-date
// by comparing the index and term of the last entries in the existing logs.
// If the logs have last entries with different terms, then the log with the
// later term is more up-to-date. If the logs end with the same term, then
// whichever log has the larger lastIndex is more up-to-date. If the logs are
// the same, the given log is up-to-date.
bool
raftLog::isUpToDate(uint64_t lasti, uint64_t term) {
  uint64_t lastT = lastTerm();
  return term > lastT || (term == lastT && lasti >= lastIndex());
}

bool
raftLog::maybeCommit(uint64_t maxIndex, uint64_t term) {
  uint64_t t;
  int err = this->term(maxIndex, &t);
  if (maxIndex > committed_ && zeroTermOnErrCompacted(t, err) == term) {
    commitTo(maxIndex);
    return true;
  }
  return false;
}

void
raftLog::restore(const Snapshot& snapshot) {
  Infof("log [%s] starts to restore snapshot [index: %llu, term: %llu]", 
    String().c_str(), snapshot.metadata().index(), snapshot.metadata().term());
  committed_ = snapshot.metadata().index();
  unstable_.restore(snapshot);
}

// append entries to unstable storage and return last index
// fatal if the first index of entries < committed_
uint64_t
raftLog::append(const EntryVec& entries) {
  if (entries.empty()) {
    return lastIndex();
  }

  uint64_t after = entries[0].index() - 1;
  if (after < committed_) {
    Fatalf("after(%llu) is out of range [committed(%llu)]", after, committed_);
  }

  unstable_.truncateAndAppend(entries);
  return lastIndex();
}

// findConflict finds the index of the conflict.
// It returns the first pair of conflicting entries between the existing
// entries and the given entries, if there are any.
// If there is no conflicting entries, and the existing entries contains
// all the given entries, zero will be returned.
// If there is no conflicting entries, but the given entries contains new
// entries, the index of the first new entry will be returned.
// An entry is considered to be conflicting if it has the same index but
// a different term.
// The first entry MUST have an index equal to the argument 'from'.
// The index of the given entries MUST be continuously increasing.
uint64_t raftLog::findConflict(const EntryVec& entries) {
  size_t i;
  for (i = 0; i < entries.size(); ++i) {
    if (!matchTerm(entries[i].index(), entries[i].term())) {
      const Entry& entry = entries[i];
      uint64_t index = entry.index();
      uint64_t term = entry.term();

      if (index <= lastIndex()) {
        uint64_t dummy;
        int err = this->term(index, &dummy);
        Infof("found conflict at index %llu [existing term: %llu, conflicting term: %llu]",
          index, zeroTermOnErrCompacted(dummy, err), term);
      }

      return index;
    }
  }

  return 0;
}

void
raftLog::unstableEntries(EntryVec *entries) {
  entries->clear();
  size_t i;
  for (i = 0; i < unstable_.entries_.size(); ++i) {
    entries->push_back(unstable_.entries_[i]);
  }
}

// nextEntries returns all the available entries for execution.
// If applied is smaller than the index of snapshot, it returns all committed
// entries after the index of snapshot.
void
raftLog::nextEntries(EntryVec* entries) {
  entries->clear();
  uint64_t offset = max(applied_ + 1, firstIndex());
  if (committed_ + 1 > offset) {
    int err = slice(offset, committed_ + 1, kNoLimit, entries);  
    if (!SUCCESS(err)) {
      Fatalf("unexpected error when getting unapplied entries (%s)", kErrString[err]);
    }
  }
}

string
raftLog::String() {
  char tmp[200];
  snprintf(tmp, sizeof(tmp), "committed=%llu, applied=%llu, unstable.offset=%llu, len(unstable.Entries)=%lu",
    committed_, applied_, unstable_.offset_, unstable_.entries_.size());

  return tmp;
}

// hasNextEntries returns if there is any available entries for execution. This
// is a fast check without heavy raftLog.slice() in raftLog.nextEnts().
bool
raftLog::hasNextEntries() {
  return committed_ + 1 > max(applied_ + 1, firstIndex());
}

int
raftLog::snapshot(Snapshot **snapshot) {
  // first check unstable storage
  if (unstable_.snapshot_ != NULL) {
    *snapshot = unstable_.snapshot_;
    return OK;
  }

  // then get snapshot from storage
  return storage_->GetSnapshot(snapshot);
}

// check err code, if success return term,
// return 0 if error code is ErrCompacted
// others Fatal
uint64_t
raftLog::zeroTermOnErrCompacted(uint64_t t, int err) {
  if (SUCCESS(err)) {
    return t;
  }

  if (err == ErrCompacted) {
    return 0;
  }

  Fatalf("unexpected error: %s", kErrString[err]);
  return 0;
}

bool
raftLog::matchTerm(uint64_t i, uint64_t term) {
  int err;
  uint64_t t;

  err = this->term(i, &t);
  if (!SUCCESS(err)) {
    return false;
  }

  return t == term;
}

// returns the term of the entry at index i, if there is any.
int
raftLog::term(uint64_t i, uint64_t *t) {
  uint64_t dummyIndex;
  int err = OK;

  *t = 0;
  // the valid term range is [index of dummy entry, last index]
  dummyIndex = firstIndex() - 1;
  if (i < dummyIndex || i > lastIndex()) {
    return OK;
  }

  // first check in unstable storage
  bool ok = unstable_.maybeTerm(i, t);
  if (ok) {
    return err;
  }

  // then check in stable storage
  err = storage_->Term(i, t);
  if (SUCCESS(err)) {
    return err;
  }

  if (err == ErrCompacted || err == ErrUnavailable) {
    return err;
  }
  Fatalf("term err:%s", kErrString[err]);

  return err;
}

uint64_t
raftLog::firstIndex() {
  uint64_t i;
  int err;

  bool ok = unstable_.maybeFirstIndex(&i);
  if (ok) {
    return i;
  }

  err = storage_->FirstIndex(&i);
  if (!SUCCESS(err)) {
    Fatalf("firstIndex error:%s", kErrString[err]);
  }

  return i;
}

uint64_t 
raftLog::lastIndex() {
  uint64_t i;
  int err;

  bool ok = unstable_.maybeLastIndex(&i);
  if (ok) {
    return i;
  }

  err = storage_->LastIndex(&i);
  if (!SUCCESS(err)) {
    Fatalf("lastIndex error:%s", kErrString[err]);
  }

  return i;
}

// slice returns a slice of log entries from lo through hi-1, inclusive.
int raftLog::slice(uint64_t lo, uint64_t hi, uint64_t maxSize, EntryVec* entries) {
  int err;

  // first check if index out of bounds
  err = mustCheckOutOfBounds(lo, hi);
  if (!SUCCESS(err)) {
    return err;
  }

  if (lo == hi) {
    return OK;
  }

  // if lo index in unstable storage
  if (lo < unstable_.offset_) {
    err = storage_->Entries(lo, min(hi,unstable_.offset_), maxSize, entries);
    if (err == ErrCompacted) {
      return err;
    } else if (err == ErrUnavailable) {
      Fatalf("entries[%llu:%llu) is unavailable from storage", lo, min(hi, unstable_.offset_));
    } else if (!SUCCESS(err)) {
      Fatalf("storage entries err:%s", kErrString[err]);
    }

    if ((uint64_t)entries->size() < min(hi, unstable_.offset_) - lo) {
      return OK;
    }
  }

  // if hi index not in unstable storage
  if (hi > unstable_.offset_) {
    EntryVec unstable;
    unstable_.slice(max(lo, unstable_.offset_), hi, &unstable);
    if (entries->size() > 0) {
      entries->insert(entries->end(), unstable.begin(), unstable.end());
    } else {
      *entries = unstable;
    }
  }

  limitSize(maxSize, entries); 
  return OK;
}

int 
raftLog::mustCheckOutOfBounds(uint64_t lo, uint64_t hi) {
  if (lo > hi) {
    Fatalf("invalid slice %llu > %llu", lo, hi);
  }

  uint64_t fi = firstIndex();
  if (lo < fi) {
    return ErrCompacted;
  }

  uint64_t li = lastIndex();
  if (hi > li + 1) {
    Fatalf("slice[%llu,%llu) out of bound [%llu,%llu]", lo, hi, fi, li);
  }

  return OK;
}

// newLog returns log using the given storage. It recovers the log to the state
// that it just commits and applies the latest snapshot.
raftLog* 
newLog(Storage *storage) {
  raftLog *log = new raftLog(storage);

  uint64_t firstIndex, lastIndex;
  int err;

  // init the first and last log index
  err = storage->FirstIndex(&firstIndex);
  if (!SUCCESS(err)) {
    Fatalf("get first index err:%s", kErrString[err]);
  }

  err = storage->LastIndex(&lastIndex);
  if (!SUCCESS(err)) {
    Fatalf("get last index err:%s", kErrString[err]);
  }

  log->unstable_.offset_ = lastIndex + 1;

  // Initialize our committed and applied pointers to the time of the last compaction.
  log->committed_ = firstIndex - 1;
  log->applied_   = firstIndex - 1;

  return log;
}

}; // namespace libraft