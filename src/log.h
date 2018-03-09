#ifndef __LOG_H__
#define __LOG_H__

#include "libraft.h"
#include "unstable_log.h"

struct raftLog {
  Storage *storage_;

  unstableLog unstable_;

  uint64_t committed_;

  uint64 applied_;

  Logger *logger_;

  raftLog(Storage *, Logger *);
  string String();

  uint64_t maybeAppend(uint64_t index, uint64_t logTerm, 
                       uint64_t committed, const vector<Entry> entries)
  bool matchTerm(uint64_t i, uint64_t term);

  int term(uint64_t i, uint64_t *t);

  uint64_t firstIndex();
  uint64_t lastIndex();
};

extern raftLog* newLog(Storage *storage, Logger *logger);

#endif // __LOG_H__
