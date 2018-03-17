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

// TestCompactionSideEffects ensures that all the log related functionality works correctly after
// a compaction.
TEST(logTests, TestCompactionSideEffects) {
  // Populate the log with 1000 entries; 750 in stable storage and 250 in unstable.
  uint64_t lastIndex = 1000;
  uint64_t unstableIndex = 750;
  uint64_t lastTerm = lastIndex;
  MemoryStorage s(&kDefaultLogger);
  int i;
  
  for (i = 1; i <= unstableIndex; ++i) {
    EntryVec entries;
    Entry entry;
    
    entry.set_index(i);
    entry.set_term(i);
    entries.push_back(entry);

    s.Append(&entries);
  }

  raftLog *log = newLog(&s, &kDefaultLogger);
  for (i = unstableIndex; i < lastIndex; ++i) {
    EntryVec entries;
    Entry entry;
    
    entry.set_index(i + 1);
    entry.set_term(i + 1);
    entries.push_back(entry);

    log->append(entries);
  }

  bool ok = log->maybeCommit(lastIndex, lastTerm);
  EXPECT_TRUE(ok);

  log->appliedTo(log->committed_);

  uint64_t offset = 500;
  s.Compact(offset);

  EXPECT_EQ(log->lastIndex(), lastIndex);

  for (i = offset; i <= log->lastIndex(); ++i) {
    uint64_t t;
    int err = log->term(i, &t);
    EXPECT_EQ(err, OK);
    EXPECT_EQ(t, i);
  }

  for (i = offset; i <= log->lastIndex(); ++i) {
    EXPECT_TRUE(log->matchTerm(i, i));
  }

  EntryVec unstableEntries;
  log->unstableEntries(&unstableEntries);
  EXPECT_EQ(unstableEntries.size(), 250);
  EXPECT_EQ(unstableEntries[0].index(), 751);

  uint64_t prev = log->lastIndex();
  {
    EntryVec entries;
    Entry entry;
    
    entry.set_index(log->lastIndex() + 1);
    entry.set_term(log->lastIndex() + 1);
    entries.push_back(entry);

    log->append(entries);
  } 
  EXPECT_EQ(log->lastIndex(), prev + 1);

  {
    EntryVec entries;
    int err = log->entries(log->lastIndex(), noLimit, &entries);
    EXPECT_EQ(err, OK);
    EXPECT_EQ(entries.size(), 1);
  }

  delete log;
}

TEST(logTests, TestHasNextEnts) {
  Snapshot sn;
  sn.mutable_metadata()->set_index(3);
  sn.mutable_metadata()->set_term(1);

  EntryVec entries;
  {
    Entry entry;
    
    entry.set_index(4);
    entry.set_term(1);
    entries.push_back(entry);
    
    entry.set_index(5);
    entry.set_term(1);
    entries.push_back(entry);
    
    entry.set_index(6);
    entry.set_term(1);
    entries.push_back(entry);
  }
  struct tmp {
    uint64_t applied;
    bool hasNext;

    tmp(uint64_t applied, bool hasnext)
      : applied(applied), hasNext(hasnext) {}
  };

  vector<tmp> tests;
  tests.push_back(tmp(0, true));
  tests.push_back(tmp(3, true));
  tests.push_back(tmp(4, true));
  tests.push_back(tmp(5, false));

  int i;
  for (i = 0; i < tests.size(); ++i) {
    MemoryStorage s(&kDefaultLogger);
    s.ApplySnapshot(sn);
    raftLog *log = newLog(&s, &kDefaultLogger);
    
    log->append(entries);
    log->maybeCommit(5, 1);
    log->appliedTo(tests[i].applied);

    EXPECT_EQ(log->hasNextEntries(), tests[i].hasNext);

    delete log;
  }
}

TEST(logTests, TestNextEnts) {
  Snapshot sn;
  sn.mutable_metadata()->set_index(3);
  sn.mutable_metadata()->set_term(1);

  EntryVec entries;
  {
    Entry entry;
    
    entry.set_index(4);
    entry.set_term(1);
    entries.push_back(entry);
    
    entry.set_index(5);
    entry.set_term(1);
    entries.push_back(entry);
    
    entry.set_index(6);
    entry.set_term(1);
    entries.push_back(entry);
  }
  struct tmp {
    uint64_t applied;
    EntryVec entries;

    tmp(uint64_t applied)
      : applied(applied) {}
  };

  vector<tmp> tests;
  {
    tmp t(0);
    t.entries.insert(t.entries.begin(), entries.begin(), entries.begin() + 2);
    tests.push_back(t);
  }
  {
    tmp t(3);
    t.entries.insert(t.entries.begin(), entries.begin(), entries.begin() + 2);
    tests.push_back(t);
  }
  {
    tmp t(4);
    t.entries.insert(t.entries.begin(), entries.begin() + 1, entries.begin() + 2);
    tests.push_back(t);
  }
  {
    tmp t(5);
    tests.push_back(t);
  }

  int i;
  for (i = 0; i < tests.size(); ++i) {
    MemoryStorage s(&kDefaultLogger);
    s.ApplySnapshot(sn);
    raftLog *log = newLog(&s, &kDefaultLogger);
    
    log->append(entries);
    log->maybeCommit(5, 1);
    log->appliedTo(tests[i].applied);

    EntryVec nextEntries;
    log->nextEntries(&nextEntries);

    EXPECT_TRUE(isDeepEqualEntries(nextEntries, tests[i].entries));

    delete log;
  }
}

// TestUnstableEnts ensures unstableEntries returns the unstable part of the
// entries correctly.
TEST(logTests, TestUnstableEnts) {
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
    uint64_t unstable;
    EntryVec entries;

    tmp(uint64_t unstable) : unstable(unstable) {}
  };

  vector<tmp> tests;
  {
    tmp t(3);
    tests.push_back(t);
  }
  {
    tmp t(1);
    t.entries = entries;
    tests.push_back(t);
  }

  int i = 0;
  for (i = 0; i < tests.size(); ++i) {
    const tmp &t = tests[i];

    // append stable entries to storage
    MemoryStorage s(&kDefaultLogger);
    {
      EntryVec ents;
      ents.insert(ents.end(), entries.begin(),  entries.begin() + t.unstable - 1);
      s.Append(&ents);
    }

    // append unstable entries to raftlog
    raftLog *log = newLog(&s, &kDefaultLogger);    
    {
      EntryVec ents;
      ents.insert(ents.end(), entries.begin() + t.unstable - 1, entries.end());
      log->append(ents);
    }

    EntryVec unstableEntries;
    log->unstableEntries(&unstableEntries);

    int len = unstableEntries.size();
    if (len > 0) {
      log->stableTo(unstableEntries[len - 1].index(), unstableEntries[len - i].term());
    }
    EXPECT_TRUE(isDeepEqualEntries(unstableEntries, t.entries)) << "i: " << i << ", size:" << unstableEntries.size();

    uint64_t w = entries[entries.size() - 1].index() + 1;
    EXPECT_EQ(log->unstable_.offset_, w);

    delete log;
  }
}
