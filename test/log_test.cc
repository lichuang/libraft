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
}
