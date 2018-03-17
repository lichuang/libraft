#include <gtest/gtest.h>
#include "libraft.h"
#include "util.h"
#include "log.h"
#include "memory_storage.h"
#include "default_logger.h"

TEST(logTests, TestFindConflict) {
  EntryVec entries;

  {
    Entry entry;

    entry.set_index(1);
    entry.set_term(1);
    entries.push_back(entry);

    entry.set_index(2);
    entry.set_term(2);
    entries.push_back(entry);

    entry.set_index(3);
    entry.set_term(3);
    entries.push_back(entry);
  }

  struct tmp {
    uint64_t wconflict;
    EntryVec entries;

    tmp(uint64_t conflict) : wconflict(conflict){}
  };

  vector<tmp> tests;
  // no conflict, empty ent
  {
    tmp t(0);
    tests.push_back(t);
  }
  // no conflict
  {
    tmp t(0);
    Entry entry;

    entry.set_index(1);
    entry.set_term(1);
    t.entries.push_back(entry);

    entry.set_index(2);
    entry.set_term(2);
    t.entries.push_back(entry);

    entry.set_index(3);
    entry.set_term(3);
    t.entries.push_back(entry);

    tests.push_back(t);
  }
  {
    tmp t(0);
    Entry entry;

    entry.set_index(2);
    entry.set_term(2);
    t.entries.push_back(entry);

    entry.set_index(3);
    entry.set_term(3);
    t.entries.push_back(entry);

    tests.push_back(t);
  }
  {
    tmp t(0);
    Entry entry;

    entry.set_index(3);
    entry.set_term(3);
    t.entries.push_back(entry);

    tests.push_back(t);
  }
  // no conflict, but has new entries
  {
    tmp t(4);
    Entry entry;

    entry.set_index(1);
    entry.set_term(1);
    t.entries.push_back(entry);

    entry.set_index(2);
    entry.set_term(2);
    t.entries.push_back(entry);

    entry.set_index(3);
    entry.set_term(3);
    t.entries.push_back(entry);

    entry.set_index(4);
    entry.set_term(4);
    t.entries.push_back(entry);

    entry.set_index(5);
    entry.set_term(4);
    t.entries.push_back(entry);

    tests.push_back(t);
  }
  {
    tmp t(4);
    Entry entry;

    entry.set_index(2);
    entry.set_term(2);
    t.entries.push_back(entry);

    entry.set_index(3);
    entry.set_term(3);
    t.entries.push_back(entry);

    entry.set_index(4);
    entry.set_term(4);
    t.entries.push_back(entry);

    entry.set_index(5);
    entry.set_term(4);
    t.entries.push_back(entry);

    tests.push_back(t);
  }
  {
    tmp t(4);
    Entry entry;

    entry.set_index(3);
    entry.set_term(3);
    t.entries.push_back(entry);

    entry.set_index(4);
    entry.set_term(4);
    t.entries.push_back(entry);

    entry.set_index(5);
    entry.set_term(4);
    t.entries.push_back(entry);

    tests.push_back(t);
  }
  {
    tmp t(4);
    Entry entry;

    entry.set_index(4);
    entry.set_term(4);
    t.entries.push_back(entry);

    entry.set_index(5);
    entry.set_term(4);
    t.entries.push_back(entry);

    tests.push_back(t);
  }
  // conflicts with existing entries
  {
    tmp t(1);
    Entry entry;

    entry.set_index(1);
    entry.set_term(4);
    t.entries.push_back(entry);

    entry.set_index(2);
    entry.set_term(4);
    t.entries.push_back(entry);

    tests.push_back(t);
  }
  {
    tmp t(2);
    Entry entry;

    entry.set_index(2);
    entry.set_term(1);
    t.entries.push_back(entry);

    entry.set_index(3);
    entry.set_term(4);
    t.entries.push_back(entry);

    entry.set_index(4);
    entry.set_term(4);
    t.entries.push_back(entry);

    tests.push_back(t);
  }
  {
    tmp t(3);
    Entry entry;

    entry.set_index(3);
    entry.set_term(1);
    t.entries.push_back(entry);

    entry.set_index(4);
    entry.set_term(2);
    t.entries.push_back(entry);

    entry.set_index(5);
    entry.set_term(4);
    t.entries.push_back(entry);

    entry.set_index(6);
    entry.set_term(4);
    t.entries.push_back(entry);
    tests.push_back(t);
  }

  int i = 0;
  for (i = 0; i < tests.size(); ++i) {
    const tmp &test = tests[i];
    MemoryStorage s(&kDefaultLogger);
    raftLog *log = newLog(&s, &kDefaultLogger);
    
    log->append(entries);

    uint64_t conflict = log->findConflict(test.entries);
    EXPECT_EQ(conflict, test.wconflict);

    delete log;
  }
}

TEST(logTests, TestIsUpToDate) {
  EntryVec entries;

  {
    Entry entry;

    entry.set_index(1);
    entry.set_term(1);
    entries.push_back(entry);

    entry.set_index(2);
    entry.set_term(2);
    entries.push_back(entry);

    entry.set_index(3);
    entry.set_term(3);
    entries.push_back(entry);
  }

  struct tmp {
    uint64_t lastindex;
    uint64_t term;
    bool isUpToDate;

    tmp(uint64_t last, uint64_t term, bool uptodate)
      : lastindex(last), term(term), isUpToDate(uptodate){}
  };

  MemoryStorage s(&kDefaultLogger);
  raftLog *log = newLog(&s, &kDefaultLogger);
  log->append(entries);

  vector<tmp> tests;
  // greater term, ignore lastIndex
  tests.push_back(tmp(log->lastIndex() - 1, 4, true));
  tests.push_back(tmp(log->lastIndex()    , 4, true));
  tests.push_back(tmp(log->lastIndex() + 1, 4, true));
  // smaller term, ignore lastIndex
  tests.push_back(tmp(log->lastIndex() - 1, 2, false));
  tests.push_back(tmp(log->lastIndex()    , 2, false));
  tests.push_back(tmp(log->lastIndex() + 1, 2, false));
  // equal term, equal or lager lastIndex wins
  tests.push_back(tmp(log->lastIndex() - 1, 3, false));
  tests.push_back(tmp(log->lastIndex()    , 3, true));
  tests.push_back(tmp(log->lastIndex() + 1, 3, true));
  int i = 0;
  for (i = 0; i < tests.size(); ++i) {
    const tmp &test = tests[i];
    bool isuptodate = log->isUpToDate(test.lastindex, test.term);
    EXPECT_EQ(isuptodate, test.isUpToDate) << "i: " << i;
  }

  delete log;
}

TEST(logTests, TestAppend) {
  EntryVec entries;

  {
    Entry entry;

    entry.set_index(1);
    entry.set_term(1);
    entries.push_back(entry);

    entry.set_index(2);
    entry.set_term(2);
    entries.push_back(entry);
  }

  struct tmp {
    uint64_t windex, wunstable;
    EntryVec entries, wentries;

    tmp(uint64_t index, uint64_t unstable) : windex(index), wunstable(unstable){}
  };
  vector<tmp> tests;
  {
    tmp t(2, 3);
    Entry entry;
    entry.set_index(1);
    entry.set_term(1);
    t.wentries.push_back(entry);
    entry.set_index(2);
    entry.set_term(2);
    t.wentries.push_back(entry);
    tests.push_back(t);
  }
  {
    tmp t(3, 3);
    Entry entry;

    entry.set_index(3);
    entry.set_term(2);
    t.entries.push_back(entry);

    entry.set_index(1);
    entry.set_term(1);
    t.wentries.push_back(entry);

    entry.set_index(2);
    entry.set_term(2);
    t.wentries.push_back(entry);

    entry.set_index(3);
    entry.set_term(2);
    t.wentries.push_back(entry);

    tests.push_back(t);
  }
  // conflicts with index 1
  {
    tmp t(1, 1);
    Entry entry;

    entry.set_index(1);
    entry.set_term(2);
    t.entries.push_back(entry);

    entry.set_index(1);
    entry.set_term(2);
    t.wentries.push_back(entry);

    tests.push_back(t);
  }
  // conflicts with index 2
  {
    tmp t(3, 2);
    Entry entry;

    entry.set_index(2);
    entry.set_term(3);
    t.entries.push_back(entry);

    entry.set_index(3);
    entry.set_term(3);
    t.entries.push_back(entry);

    entry.set_index(1);
    entry.set_term(1);
    t.wentries.push_back(entry);

    entry.set_index(2);
    entry.set_term(3);
    t.wentries.push_back(entry);

    entry.set_index(3);
    entry.set_term(3);
    t.wentries.push_back(entry);
    tests.push_back(t);
  }
  int i = 0;
  for (i = 0; i < tests.size(); ++i) {
    const tmp &test = tests[i];
    MemoryStorage s(&kDefaultLogger);
    s.Append(&entries);
    raftLog *log = newLog(&s, &kDefaultLogger);
    
    uint64_t index = log->append(test.entries);

    EXPECT_EQ(index, test.windex);

    EntryVec entries;
    int err = log->entries(1, noLimit, &entries);
    EXPECT_EQ(err, OK);
    EXPECT_TRUE(isDeepEqualEntries(entries, test.wentries));
    EXPECT_EQ(log->unstable_.offset_, test.wunstable);
    delete log;
  }
}

// TestLogMaybeAppend ensures:
// If the given (index, term) matches with the existing log:
//  1. If an existing entry conflicts with a new one (same index
//  but different terms), delete the existing entry and all that
//  follow it
//  2.Append any new entries not already in the log
// If the given (index, term) does not match with the existing log:
//  return false
TEST(logTests, TestLogMaybeAppend) {
  EntryVec entries;
  uint64_t lastindex = 3;
  uint64_t lastterm = 3;
  uint64_t commit = 1;

  {
    Entry entry;

    entry.set_index(1);
    entry.set_term(1);
    entries.push_back(entry);

    entry.set_index(2);
    entry.set_term(2);
    entries.push_back(entry);

    entry.set_index(3);
    entry.set_term(3);
    entries.push_back(entry);
  }
  struct tmp {
    uint64_t logTerm;
    uint64_t index;
    uint64_t commited;
    EntryVec entries;

    uint64_t lasti;
    bool wappend;
    uint64_t wcommit;
    bool wpanic;

    tmp(uint64_t logterm, uint64_t index, uint64_t commited,
        uint64_t lasti, bool append, uint64_t commit, bool panic)
      : logTerm(logterm), index(index), commited(commited),
        lasti(lasti), wappend(append), wcommit(commit), wpanic(panic) {}
  };

  vector<tmp> tests;
  // not match: term is different
  {
    tmp t(lastterm - 1, lastindex, lastindex, 0, false, commit, false);
    Entry entry;

    entry.set_index(lastindex + 1);
    entry.set_term(4);
    t.entries.push_back(entry);
    tests.push_back(t);
  }
  // not match: index out of bound
  {
    tmp t(lastterm, lastindex + 1, lastindex, 0, false, commit, false);
    Entry entry;

    entry.set_index(lastindex + 2);
    entry.set_term(4);
    t.entries.push_back(entry);
    tests.push_back(t);
  }
  // match with the last existing entry
  {
    tmp t(lastterm, lastindex, lastindex, lastindex, true, lastindex, false);

    tests.push_back(t);
  }
  {
    // do not increase commit higher than lastnewi
    tmp t(lastterm, lastindex, lastindex + 1, lastindex, true, lastindex, false);

    tests.push_back(t);
  }
  {
    // commit up to the commit in the message
    tmp t(lastterm, lastindex, lastindex - 1, lastindex, true, lastindex - 1, false);

    tests.push_back(t);
  }
  {
    // commit do not decrease
    tmp t(lastterm, lastindex, 0, lastindex, true, commit, false);

    tests.push_back(t);
  }
  {
    // commit do not decrease
    tmp t(0, 0, lastindex, 0, true, commit, false);

    tests.push_back(t);
  }
  {
    tmp t(lastterm, lastindex, lastindex, lastindex + 1, true, lastindex, false);

    Entry entry;

    entry.set_index(lastindex + 1);
    entry.set_term(4);
    t.entries.push_back(entry);
    tests.push_back(t);
  }
  {
    // do not increase commit higher than lastnewi
    tmp t(lastterm, lastindex, lastindex + 2, lastindex + 1, true, lastindex + 1, false);

    Entry entry;

    entry.set_index(lastindex + 1);
    entry.set_term(4);
    t.entries.push_back(entry);
    tests.push_back(t);
  }
  {
    tmp t(lastterm, lastindex, lastindex + 2, lastindex + 2, true, lastindex + 2, false);

    Entry entry;

    entry.set_index(lastindex + 1);
    entry.set_term(4);
    t.entries.push_back(entry);

    entry.set_index(lastindex + 2);
    entry.set_term(4);
    t.entries.push_back(entry);
    tests.push_back(t);
  }
  // match with the the entry in the middle
  {
    tmp t(lastterm-1, lastindex-1, lastindex, lastindex, true, lastindex, false);

    Entry entry;

    entry.set_index(lastindex);
    entry.set_term(4);
    t.entries.push_back(entry);

    tests.push_back(t);
  }
  {
    tmp t(lastterm-2, lastindex-2, lastindex, lastindex-1, true, lastindex-1, false);

    Entry entry;

    entry.set_index(lastindex-1);
    entry.set_term(4);
    t.entries.push_back(entry);

    tests.push_back(t);
  }
  // conflict with existing committed entry
  /*
  {
    tmp t(lastterm-3, lastindex-3, lastindex, lastindex-2, true, lastindex-2, true);

    Entry entry;

    entry.set_index(lastindex-2);
    entry.set_term(4);
    t.entries.push_back(entry);

    tests.push_back(t);
  }
  */
  {
    tmp t(lastterm-2, lastindex-2, lastindex, lastindex, true, lastindex, false);

    Entry entry;

    entry.set_index(lastindex-1);
    entry.set_term(4);
    t.entries.push_back(entry);

    entry.set_index(lastindex);
    entry.set_term(4);
    t.entries.push_back(entry);
    tests.push_back(t);
  }
  int i = 0;
  for (i = 0; i < tests.size(); ++i) {
    const tmp &test = tests[i];
    MemoryStorage s(&kDefaultLogger);
    raftLog *log = newLog(&s, &kDefaultLogger);
    log->append(entries);
    log->committed_ = commit;

    uint64_t glasti;
    bool ok  = log->maybeAppend(test.index, test.logTerm, test.commited, test.entries, &glasti);
    uint64_t gcommit = log->committed_;

    EXPECT_EQ(glasti, test.lasti);
    EXPECT_EQ(ok, test.wappend) << "i: " << i;
    EXPECT_EQ(gcommit, test.wcommit);
    if (glasti > 0 && test.entries.size() != 0) {
      EntryVec entries;
      int err = log->slice(log->lastIndex() - test.entries.size() + 1, log->lastIndex() + 1, noLimit, &entries);
      EXPECT_EQ(err, OK);
      EXPECT_TRUE(isDeepEqualEntries(test.entries, entries));
    }
    delete log;
  }
}
