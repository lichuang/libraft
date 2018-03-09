#include "log.h"

raftLog* newLog(Storage *storage, Logger *logger) {
  raftLog *log = new raftLog(storage, logger);

  uint64_t firstIndex, lastIndex;
  int err;

  err = storage->FirstIndex(&firstIndex);
  if (!SUCCESS(err)) {
    logger->Fatalf("get first index err:%s", GetErrorString(err));
  }

  err = storage->LastIndex(&lastIndex);
  if (!SUCCESS(err)) {
    logger->Fatalf("get last index err:%s", GetErrorString(err));
  }

  log->unstable_.offset_ = lastIndex + 1;
  log->unstable_.logger_ = logger;

  log->committed_ = firstIndex - 1;
  log.applied_    = firstIndex - 1;

  return log;
}

raftLog::raftLog(Storage *storage, Logger *logger) 
  : storage_(storage)
  , logger_(logger) {
}

// maybeAppend returns 0 if the entries cannot be appended. Otherwise,
// it returns last index of new entries.
uint64_t raftLog::maybeAppend(uint64_t index, uint64_t logTerm, 
                              uint64_t committed, const vector<Entry>& entries) {
  if (!matchTerm(index, logTerm)) {
    return 0;
  }

  uint64_t lastNewI, ci, offset;

  lastNewI = index + (uint64_t)entries.size();
  ci = findConflict(entries);

  if (ci == 0 || ci <= committed_) {
    logger_->Fatalf("entry %d conflict with committed entry [committed(%d)]", ci, l.committed);
  }

  offset = index + 1;
  vector<Entry> appendEntries(entries.begin() + ci - offset);
  append(appendEntries);
  commitTo(min(committed_, lastNewI));
  return lastNewI;
}

void raftLog::commitTo(uint64_t tocommit) {
  // never decrease commit
  if (committed_ >= tocommit) {
    return;
  }

  if (lastIndex() < tocommit) {
    logger_->Fatalf("tocommit(%d) is out of range [lastIndex(%d)]. Was the raft log corrupted, truncated, or lost?",
      tocommit, lastIndex);
  }

  committed_ = tocommit;
}

// append entries to unstable storage and return last index
// fatal if the first index of entries < committed_
uint64_t raftLog::append(vector<Entry>& entries) {
  if (entries.size() == 0) {
    return lastIndex();
  }

  uint64_t after = entries[0].index() - 1;
  if (after < committed_) {
    logger_->Fatalf("after(%d) is out of range [committed(%d)]", after, committed_);
  }

  unstable_.truncateAndAppend(entries);
  return lastIndex();
}

uint64_t raftLog::findConflict(const vector<Entry>& entries) {
  int i;
  for (i = 0; i < entries.size(); ++i) {
    if (!matchTerm(entries[i].index(), entries[i].term())) {
      const Entry& entry = entries[i];
      uint64_t index = entry.index();
      uint64_t term = entry.term();

      if (index <= lastIndex()) {
        uint64_t dummy;
        int err = term(index, &dummy);
        logger_->Infof("found conflict at index %d [existing term: %d, conflicting term: %d]",
          index, zeroTermOnErrCompacted(dummy, err), term);
      }

      return index;
    }
  }

  return 0;
}

uint64_t raftLog::zeroTermOnErrCompacted(uint64_t t, int err) {
  if (SUCCESS(err)) {
    return t;
  }

  if (err == ErrCompacted) {
    return 0;
  }

  logger_->Fatalf("unexpected error: %s", GetErrorString(err));
  return 0;
}


bool raftLog::matchTerm(uint64_t i, uint64_t term) {
  int err;
  uint64_t t;

  err = term(i, &t);
  if (!SUCCESS(err)) {
    return false;
  }

  return t == term;
}

int raftLog::term(uint64_t i, uint64_t *t) {
  uint64_t dummyIndex, term = 0;
  int err = Success;

  dummyIndex = firstIndex() - 1;
  if (i < dummyIndex || i > lastIndex()) {
    return 0;
  }

  term = unstable_.maybeTerm(i);
  if (term > 0) {
    goto out;
  }

  err = storage_->Term(i);
  if (SUCCESS(err)) {
    goto out;
  }

  if (err == ErrCompacted || err == ErrUnavailable) {
    goto out;
  }
  logger_->Fatalf("term err:%s", GetErrorString(err));
out:
  *t = term;
  return err;
}

uint64_t raftLog::firstIndex() {
  uint64_t i;
  int err;

  i = unstable_.maybeFirstIndex();
  if (i) {
    return i;
  }

  err = storage_.FirstIndex(&i);
  if (!SUCCESS(err)) {
    logger_.Fatalf("firstIndex error:%s", GetErrorString(err));
  }

  return i;
}

uint64_t raftLog::lastIndex() {
  uint64_t i;

  i = unstable_.maybeLastIndex();
  if (i > 0) {
    return i;
  }

  err = storage_.LastIndex(&i);
  if (!SUCCESS(err)) {
    logger_.Fatalf("lastIndex error:%s", GetErrorString(err));
  }

  return i;
}

const vector<Entry>* raftLog::unstableEntries() {
  if (unstable_.entries_.size() == 0) {
    return NULL;
  }

  return &unstable_.entries_;
}

int raftLog::slice(uint64_t lo, uint64_t hi, uint64_t maxSize, vector<Entry> *entries) {
  if (lo == hi) {
    return Success;
  }


}

// nextEnts returns all the available entries for execution.
// If applied is smaller than the index of snapshot, it returns all committed
// entries after the index of snapshot.

