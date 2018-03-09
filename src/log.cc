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

uint64_t raftLog::maybeAppend(uint64_t index, uint64_t logTerm, 
                              uint64_t committed, const vector<Entry> entries) {

}

bool raftLog::matchTerm(uint64_t i, uint64_t term) {

}

int raftLog::term(uint64_t i, uint64_t *t) {
  dummyIndex = 
}

uint64_t raftLog::firstIndex() {
  uint64_t i;

  i = unstable_.maybeFirstIndex();
  if (!i) {
    return 
  }
}
