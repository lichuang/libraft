/*
 * Copyright (C) lichuang
 */

#include <gtest/gtest.h>
#include "libraft.h"
#include "raft_test_util.h"
#include "base/default_logger.h"
#include "base/util.h"
#include "storage/log.h"
#include "storage/memory_storage.h"

using namespace libraft;

TEST(logTests, TestFindConflict) {
  // first fill up previous entries
  EntryVec previousEnts = {
    initEntry(1,1),
    initEntry(2,2),
    initEntry(3,3),
  };

  // then add test cases
  struct tmp {
    uint64_t wconflict;
    EntryVec entries;
  } tests[] = {
    // no conflict, empty ent
    {.wconflict = 0,},
    // no conflict
    {.wconflict = 0, .entries = {initEntry(1,1), initEntry(2, 2), initEntry(3,3)}},
    {.wconflict = 0, .entries = {initEntry(2,2), initEntry(3,3)}},
    {.wconflict = 0, .entries = {initEntry(3,3)}},
    // no conflict, but has new entries
    {.wconflict = 4, .entries = {initEntry(1,1), initEntry(2, 2), initEntry(3,3), initEntry(4,4), initEntry(5,4)}},
    {.wconflict = 4, .entries = {initEntry(2, 2), initEntry(3,3), initEntry(4,4), initEntry(5,4)}},
    {.wconflict = 4, .entries = {initEntry(3,3), initEntry(4,4), initEntry(5,4)}},
    {.wconflict = 4, .entries = {initEntry(4,4), initEntry(5,4)}},
    // conflicts with existing entries
    {.wconflict = 1, .entries = {initEntry(1,4), initEntry(2,4)}},  
    {.wconflict = 2, .entries = {initEntry(2,1), initEntry(3,4), initEntry(4,4)}},
    {.wconflict = 3, .entries = {initEntry(3,1), initEntry(4,2), initEntry(5,4), initEntry(6,4)}},  
  };

  size_t i = 0;
  for (i = 0; i < SIZEOF_ARRAY(tests); i++) {
    const tmp &test = tests[i];
    MemoryStorage s(&kDefaultLogger);
    raftLog *log = newLog(&s, &kDefaultLogger);
    
    log->append(previousEnts);

    uint64_t conflict = log->findConflict(test.entries);
    EXPECT_EQ(conflict, test.wconflict);

    delete log;
  }
}

TEST(logTests, TestIsUpToDate) {
  // first fill up previous entries
  EntryVec previousEnts = {
    initEntry(1,1),
    initEntry(2,2),
    initEntry(3,3),
  };

  MemoryStorage s(&kDefaultLogger);
  raftLog *log = newLog(&s, &kDefaultLogger);
  log->append(previousEnts);

  // then add test cases
  struct tmp {
    uint64_t lastindex;
    uint64_t term;
    bool isUpToDate;
  } tests[] = {
    // greater term, ignore lastIndex
    {.lastindex = log->lastIndex() - 1, .term = 4, .isUpToDate = true},
    {.lastindex = log->lastIndex(),     .term = 4, .isUpToDate = true},
    {.lastindex = log->lastIndex() + 1, .term = 4, .isUpToDate = true},
    // smaller term, ignore lastIndex
    {.lastindex = log->lastIndex() - 1, .term = 2, .isUpToDate = false},
    {.lastindex = log->lastIndex(),     .term = 2, .isUpToDate = false},
    {.lastindex = log->lastIndex() + 1, .term = 2, .isUpToDate = false},
    // equal term, equal or lager lastIndex wins
    {.lastindex = log->lastIndex() - 1, .term = 3, .isUpToDate = false},
    {.lastindex = log->lastIndex(),     .term = 3, .isUpToDate = true},
    {.lastindex = log->lastIndex() + 1, .term = 3, .isUpToDate = true},
  };

  size_t i = 0;
  for (i = 0; i < SIZEOF_ARRAY(tests); ++i) {
    const tmp &test = tests[i];
    bool isuptodate = log->isUpToDate(test.lastindex, test.term);
    EXPECT_EQ(isuptodate, test.isUpToDate) << "i: " << i;
  }

  delete log;
}

TEST(logTests, TestAppend) {
  // first fill up previous entries
  EntryVec previousEnts = {
    initEntry(1,1),
    initEntry(2,2),
  };

  // then add test cases
  struct tmp {
    EntryVec entries;
    uint64_t windex;
    EntryVec wentries;
    uint64_t wunstable;       
  } tests[] = {
    {
      .entries = {}, 
      .windex = 2, 
      .wentries = {initEntry(1,1), initEntry(2,2)}, 
      .wunstable = 3,
    },
    {
      .entries = {initEntry(3,2)}, 
      .windex = 3, 
      .wentries = {initEntry(1,1), initEntry(2,2), initEntry(3,2)},
      .wunstable = 3,
    },
    // conflicts with index 1
    {
      .entries = {initEntry(1,2)}, 
      .windex = 1, 
      .wentries = {initEntry(1,2),},
      .wunstable = 1,
    },
    // conflicts with index 2
    {
      .entries = {initEntry(2,3), initEntry(3,3)}, 
      .windex = 3, 
      .wentries = {initEntry(1,1), initEntry(2,3), initEntry(3,3)},
      .wunstable = 2,
    },    
  };

  size_t i = 0;
  for (i = 0; i < SIZEOF_ARRAY(tests); ++i) {
    const tmp &test = tests[i];
    MemoryStorage s(&kDefaultLogger);
    s.Append(previousEnts);
    raftLog *log = newLog(&s, &kDefaultLogger);
    
    uint64_t index = log->append(test.entries);

    EXPECT_EQ(index, test.windex);

    EntryVec ret_entries;
    int err = log->entries(1, kNoLimit, &ret_entries);
    EXPECT_EQ(err, OK);
    EXPECT_TRUE(isDeepEqualEntries(ret_entries, test.wentries));
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
  EntryVec previousEnts;
  uint64_t lastindex = 3;
  uint64_t lastterm = 3;
  uint64_t commit = 1;

  {
    Entry entry;

    entry.set_index(1);
    entry.set_term(1);
    previousEnts.push_back(entry);

    entry.set_index(2);
    entry.set_term(2);
    previousEnts.push_back(entry);

    entry.set_index(3);
    entry.set_term(3);
    previousEnts.push_back(entry);
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
  size_t i = 0;
  for (i = 0; i < tests.size(); ++i) {
    const tmp &test = tests[i];
    MemoryStorage s(&kDefaultLogger);
    raftLog *log = newLog(&s, &kDefaultLogger);
    log->append(previousEnts);
    log->committed_ = commit;

    uint64_t glasti;
    bool ok  = log->maybeAppend(test.index, test.logTerm, test.commited, test.entries, &glasti);
    uint64_t gcommit = log->committed_;

    EXPECT_EQ(glasti, test.lasti);
    EXPECT_EQ(ok, test.wappend) << "i: " << i;
    EXPECT_EQ(gcommit, test.wcommit);
    if (glasti > 0 && test.entries.size() != 0) {
      EntryVec ret_entries;
      int err = log->slice(log->lastIndex() - test.entries.size() + 1, log->lastIndex() + 1, kNoLimit, &ret_entries);
      EXPECT_EQ(err, OK);
      EXPECT_TRUE(isDeepEqualEntries(test.entries, ret_entries));
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
  size_t i;
  
  for (i = 1; i <= unstableIndex; ++i) {
    EntryVec entries;
    Entry entry;
    
    entry.set_index(i);
    entry.set_term(i);
    entries.push_back(entry);

    s.Append(entries);
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
  EXPECT_EQ((int)unstableEntries.size(), 250);
  EXPECT_EQ((int)unstableEntries[0].index(), 751);

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
    int err = log->entries(log->lastIndex(), kNoLimit, &entries);
    EXPECT_EQ(err, OK);
    EXPECT_EQ((int)entries.size(), 1);
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

  size_t i;
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

  size_t i;
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
  EntryVec previousEnts;

  {
    Entry entry;

    entry.set_index(1);
    entry.set_term(1);
    previousEnts.push_back(entry);

    entry.set_index(2);
    entry.set_term(2);
    previousEnts.push_back(entry);
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
    t.entries = previousEnts;
    tests.push_back(t);
  }

  size_t i = 0;
  for (i = 0; i < tests.size(); ++i) {
    const tmp &t = tests[i];

    // append stable entries to storage
    MemoryStorage s(&kDefaultLogger);
    {
      EntryVec ents;
      ents.insert(ents.end(), previousEnts.begin(),  previousEnts.begin() + t.unstable - 1);
      s.Append(ents);
    }

    // append unstable entries to raftlog
    raftLog *log = newLog(&s, &kDefaultLogger);    
    {
      EntryVec ents;
      ents.insert(ents.end(), previousEnts.begin() + t.unstable - 1, previousEnts.end());
      log->append(ents);
    }

    EntryVec unstableEntries;
    log->unstableEntries(&unstableEntries);

    int len = unstableEntries.size();
    if (len > 0) {
      log->stableTo(unstableEntries[len - 1].index(), unstableEntries[len - i].term());
    }
    EXPECT_TRUE(isDeepEqualEntries(unstableEntries, t.entries)) << "i: " << i << ", size:" << unstableEntries.size();

    uint64_t w = previousEnts[previousEnts.size() - 1].index() + 1;
    EXPECT_EQ(log->unstable_.offset_, w);

    delete log;
  }
}

TEST(logTests, TestCommitTo) {
  EntryVec previousEnts;
  uint64_t commit = 2;

  {
    Entry entry;

    entry.set_index(1);
    entry.set_term(1);
    previousEnts.push_back(entry);

    entry.set_index(2);
    entry.set_term(2);
    previousEnts.push_back(entry);

    entry.set_index(3);
    entry.set_term(3);
    previousEnts.push_back(entry);
  }
  struct tmp {
    uint64_t commit, wcommit;
    bool panic;

    tmp(uint64_t commit, uint64_t wcommit, bool panic)
      : commit(commit), wcommit(wcommit), panic(panic) {}
  };

  vector<tmp> tests;
  tests.push_back(tmp(3,3,false));
  tests.push_back(tmp(1,2,false));  // never decrease
  //tests.push_back(tmp(4,0,true));   // commit out of range -> panic

  size_t i;
  for (i = 0; i < tests.size(); ++i) {
    const tmp &t = tests[i];

    MemoryStorage s(&kDefaultLogger);
    raftLog *log = newLog(&s, &kDefaultLogger);

    log->append(previousEnts);
    log->committed_ = commit;
    log->commitTo(t.commit);
    EXPECT_EQ(log->committed_, t.wcommit);
    delete log;
  }
}

TEST(logTests, TestStableTo) {
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
    uint64_t stablei, stablet, wunstable;

    tmp(uint64_t stablei, uint64_t stablet, uint64_t wunstable)
      : stablei(stablei), stablet(stablet), wunstable(wunstable) {}
  };

  vector<tmp> tests;
  tests.push_back(tmp(1,1,2));
  tests.push_back(tmp(2,2,3));
  tests.push_back(tmp(2,1,1));  // bad term
  tests.push_back(tmp(3,1,1));  // bad index

  size_t i;
  for (i = 0; i < tests.size(); ++i) {
    const tmp &t = tests[i];

    MemoryStorage s(&kDefaultLogger);
    raftLog *log = newLog(&s, &kDefaultLogger);

    log->append(entries);
    log->stableTo(t.stablei, t.stablet);
    EXPECT_EQ(log->unstable_.offset_, t.wunstable);
    delete log;
  }
}

TEST(logTests, TestStableToWithSnap) {
  uint64_t snapi = 5, snapt = 2;

  struct tmp {
    uint64_t stablei, stablet, wunstable;
    EntryVec entries;

    tmp(uint64_t stablei, uint64_t stablet, uint64_t wunstable)
      : stablei(stablei), stablet(stablet), wunstable(wunstable) {}
  };

  vector<tmp> tests;
  {
    tmp t(snapi + 1, snapt, snapi + 1);
    tests.push_back(t);
  }
  {
    tmp t(snapi, snapt + 1, snapi + 1);
    tests.push_back(t);
  }
  {
    tmp t(snapi - 1, snapt, snapi + 1);
    tests.push_back(t);
  }
  {
    tmp t(snapi + 1, snapt + 1, snapi + 1);
    tests.push_back(t);
  }
  {
    tmp t(snapi, snapt + 1, snapi + 1);
    tests.push_back(t);
  }
  {
    tmp t(snapi - 1, snapt + 1, snapi + 1);
    tests.push_back(t);
  }
  {
    tmp t(snapi + 1, snapt, snapi + 2);
    Entry entry;
    entry.set_index(snapi + 1);
    entry.set_term(snapt);
    t.entries.push_back(entry);
    tests.push_back(t);
  }
  {
    tmp t(snapi, snapt, snapi + 1);
    Entry entry;
    entry.set_index(snapi + 1);
    entry.set_term(snapt);
    t.entries.push_back(entry);
    tests.push_back(t);
  }
  {
    tmp t(snapi - 1, snapt, snapi + 1);
    Entry entry;
    entry.set_index(snapi + 1);
    entry.set_term(snapt);
    t.entries.push_back(entry);
    tests.push_back(t);
  }
  {
    tmp t(snapi + 1, snapt + 1, snapi + 1);
    Entry entry;
    entry.set_index(snapi + 1);
    entry.set_term(snapt);
    t.entries.push_back(entry);
    tests.push_back(t);
  }
  {
    tmp t(snapi, snapt + 1, snapi + 1);
    Entry entry;
    entry.set_index(snapi + 1);
    entry.set_term(snapt);
    t.entries.push_back(entry);
    tests.push_back(t);
  }
  {
    tmp t(snapi - 1, snapt + 1, snapi + 1);
    Entry entry;
    entry.set_index(snapi + 1);
    entry.set_term(snapt);
    t.entries.push_back(entry);
    tests.push_back(t);
  }

  size_t i = 0;
  for (i = 0; i < tests.size(); ++i) {
    const tmp &t = tests[i];

    MemoryStorage s(&kDefaultLogger);

    Snapshot sn;
    sn.mutable_metadata()->set_index(snapi);
    sn.mutable_metadata()->set_term(snapt);
    s.ApplySnapshot(sn);

    raftLog *log = newLog(&s, &kDefaultLogger);
    log->append(t.entries);
    log->stableTo(t.stablei, t.stablet);
    EXPECT_EQ(log->unstable_.offset_, t.wunstable);
    delete log;
  }
}

//TestCompaction ensures that the number of log entries is correct after compactions.
TEST(logTests, TestCompaction) {
  struct tmp {
    uint64_t lastIndex;
    vector<uint64_t> compact;
    vector<int> wleft;
    bool wallow;

    tmp(uint64_t i, bool allow)
      : lastIndex(i), wallow(allow) {}
  };

  vector<tmp> tests;
  {
    // out of upper bound
    tmp t(1000, false);
    t.compact.push_back(1001);
    t.wleft.push_back(-1);

    //tests.push_back(t);
  }
  {
    // out of upper bound
    tmp t(1000, true);
    t.compact.push_back(300);
    t.compact.push_back(500);
    t.compact.push_back(800);
    t.compact.push_back(900);
    t.wleft.push_back(700);
    t.wleft.push_back(500);
    t.wleft.push_back(200);
    t.wleft.push_back(100);

    tests.push_back(t);
  }

  size_t i = 0;
  for (i = 0; i < tests.size(); ++i) {
    const tmp &t = tests[i];
    size_t j = 0;

    MemoryStorage s(&kDefaultLogger);
    EntryVec entries;
    for (j = 1; j <= t.lastIndex; ++j) {
      Entry entry;
      entry.set_index(j);
      entries.push_back(entry);
    }
    s.Append(entries);

    raftLog *log = newLog(&s, &kDefaultLogger);
    log->maybeCommit(t.lastIndex, 0);
    log->appliedTo(log->committed_);

    for (j = 0; j < t.compact.size(); ++j) {
      int err = s.Compact(t.compact[j]);
      if (!SUCCESS(err)) {
        EXPECT_TRUE(t.wallow);
        continue;
      }
      EntryVec all;
      log->allEntries(&all);
      EXPECT_EQ(t.wleft[j], (int)all.size());
    }
    delete log;
  }
}

TEST(logTests, TestLogRestore) {
  uint64_t index = 1000;
  uint64_t term  = 1000;

  Snapshot sn;
  sn.mutable_metadata()->set_index(index);
  sn.mutable_metadata()->set_term(term);

  MemoryStorage s(&kDefaultLogger);
  s.ApplySnapshot(sn);

  raftLog *log = newLog(&s, &kDefaultLogger);

  EntryVec allEntries;
  log->allEntries(&allEntries);

  EXPECT_TRUE(allEntries.empty());
  EXPECT_EQ(log->firstIndex(), index + 1);
  EXPECT_EQ(log->committed_, index);
  EXPECT_EQ(log->unstable_.offset_, index + 1);
  uint64_t t;
  int err = log->term(index, &t);
  EXPECT_TRUE(SUCCESS(err));
  EXPECT_EQ(t, term);

  delete log;
}

TEST(logTests, TestIsOutOfBounds) {
  uint64_t offset = 100;
  uint64_t num  = 100;

  Snapshot sn;
  sn.mutable_metadata()->set_index(offset);

  MemoryStorage s(&kDefaultLogger);
  s.ApplySnapshot(sn);

  raftLog *log = newLog(&s, &kDefaultLogger);

  size_t i = 0;
  EntryVec entries;
  for (i = 1; i <= num; ++i) {
    Entry entry;
    entry.set_index(i + offset);
    entries.push_back(entry);
  }
  log->append(entries);

  uint64_t first = offset + 1;

  struct tmp {
    uint64_t lo, hi;
    bool wpanic;
    bool errCompacted;

    tmp(uint64_t lo, uint64_t hi, bool panic, bool err)
      : lo(lo), hi(hi), wpanic(panic), errCompacted(err) {
    }
  };

  vector<tmp> tests;
  tests.push_back(tmp(first - 2, first + 1, false, true));
  tests.push_back(tmp(first - 1, first + 1, false, true));
  tests.push_back(tmp(first,     first    , false, false));
  tests.push_back(tmp(first + num/2, first + num/2, false, false));
  tests.push_back(tmp(first + num - 1, first + num - 1, false, false));
  tests.push_back(tmp(first + num, first + num, false, false));

  for (i = 0; i < tests.size(); ++i) {
    const tmp &t = tests[i];

    int err = log->mustCheckOutOfBounds(t.lo, t.hi);
    EXPECT_FALSE(t.errCompacted && err != ErrCompacted);
    EXPECT_FALSE(!t.errCompacted && !SUCCESS(err));
  }

  delete log;
}

TEST(logTests, TestTerm) {
  uint64_t offset = 100;
  uint64_t num  = 100;

  Snapshot sn;
  sn.mutable_metadata()->set_index(offset);
  sn.mutable_metadata()->set_term(1);

  MemoryStorage s(&kDefaultLogger);
  s.ApplySnapshot(sn);

  raftLog *log = newLog(&s, &kDefaultLogger);

  size_t i = 0;
  EntryVec entries;
  for (i = 1; i < num; ++i) {
    Entry entry;
    entry.set_index(i + offset);
    entry.set_term(i);
    entries.push_back(entry);
  }
  log->append(entries);

  struct tmp {
    uint64_t index, w;

    tmp(uint64_t index, uint64_t w)
      : index(index), w(w) {}
  };

  vector<tmp> tests;
  tests.push_back(tmp(offset - 1, 0));
  tests.push_back(tmp(offset,     1));
  tests.push_back(tmp(offset + num/2, num/2));
  tests.push_back(tmp(offset + num - 1, num - 1));
  tests.push_back(tmp(offset + num, 0));

  for (i = 0; i < tests.size(); ++i) {
    const tmp &t = tests[i];
    uint64_t tm;
    int err = log->term(t.index, &tm);
    EXPECT_TRUE(SUCCESS(err));
    EXPECT_EQ(tm, t.w);
  }

  delete log;
}

TEST(logTests, TestTermWithUnstableSnapshot) {
  uint64_t storagesnapi = 100;
  uint64_t unstablesnapi = storagesnapi + 5;

  Snapshot sn;
  sn.mutable_metadata()->set_index(storagesnapi);
  sn.mutable_metadata()->set_term(1);

  MemoryStorage s(&kDefaultLogger);
  s.ApplySnapshot(sn);

  raftLog *log = newLog(&s, &kDefaultLogger);
  {
    Snapshot tmp_sn;
    tmp_sn.mutable_metadata()->set_index(unstablesnapi);
    tmp_sn.mutable_metadata()->set_term(1);

    log->restore(tmp_sn);
  }
  size_t i = 0;

  struct tmp {
    uint64_t index, w;

    tmp(uint64_t index, uint64_t w)
      : index(index), w(w) {}
  };

  vector<tmp> tests;
  // cannot get term from storage
  tests.push_back(tmp(storagesnapi, 0));
  // cannot get term from the gap between storage ents and unstable snapshot
  tests.push_back(tmp(storagesnapi + 1, 0));
  tests.push_back(tmp(unstablesnapi - 1, 0));
  // get term from unstable snapshot index
  tests.push_back(tmp(unstablesnapi, 1));

  for (i = 0; i < tests.size(); ++i) {
    const tmp &t = tests[i];
    uint64_t tm;
    int err = log->term(t.index, &tm);
    EXPECT_TRUE(SUCCESS(err));
    EXPECT_EQ(tm, t.w) << "i: " << i;
  }

  delete log;
}

TEST(logTests, TestSlice) {
  uint64_t offset = 100;
  uint64_t num = 100;
  uint64_t last = offset + num;
  uint64_t half = offset + num / 2;

  Entry halfe;
  halfe.set_index(half);
  halfe.set_term(half);

  Snapshot sn;
  sn.mutable_metadata()->set_index(offset);

  MemoryStorage s(&kDefaultLogger);
  s.ApplySnapshot(sn);

  raftLog *log = newLog(&s, &kDefaultLogger);

  size_t i = 0;
  EntryVec entries;
  for (i = 1; i < num; ++i) {
    Entry entry;
    entry.set_index(i + offset);
    entry.set_term(i + offset);
    entries.push_back(entry);
  }
  log->append(entries);

  struct tmp {
    uint64_t from, to, limit;
    EntryVec entries;
    bool wpanic;

    tmp(uint64_t from, uint64_t to, uint64_t limit, bool panic)
      : from(from), to(to), limit(limit), wpanic(panic) {}
  };

  vector<tmp> tests;
  // test no limit
  {
    tmp t(offset - 1, offset + 1, kNoLimit, false);
    tests.push_back(t);
  }
  {
    tmp t(offset, offset + 1, kNoLimit, false);
    tests.push_back(t);
  }
  {
    tmp t(half - 1, half + 1, kNoLimit, false);
    Entry entry;

    entry.set_index(half - 1);
    entry.set_term(half - 1);
    t.entries.push_back(entry);

    entry.set_index(half);
    entry.set_term(half);
    t.entries.push_back(entry);

    tests.push_back(t);
  }
  {
    tmp t(half, half + 1, kNoLimit, false);
    Entry entry;

    entry.set_index(half);
    entry.set_term(half);
    t.entries.push_back(entry);

    tests.push_back(t);
  }
  {
    tmp t(last - 1, last, kNoLimit, false);
    Entry entry;

    entry.set_index(last - 1);
    entry.set_term(last - 1);
    t.entries.push_back(entry);

    tests.push_back(t);
  }
  // test limit
  {
    tmp t(half - 1, half + 1, 0, false);
    Entry entry;

    entry.set_index(half - 1);
    entry.set_term(half - 1);
    t.entries.push_back(entry);

    tests.push_back(t);
  }
  {
    tmp t(half - 1, half + 1, halfe.ByteSizeLong() + 1, false);
    Entry entry;

    entry.set_index(half - 1);
    entry.set_term(half - 1);
    t.entries.push_back(entry);

    tests.push_back(t);
  }
  {
    tmp t(half - 2, half + 1, halfe.ByteSizeLong() + 1, false);
    Entry entry;

    entry.set_index(half - 2);
    entry.set_term(half - 2);
    t.entries.push_back(entry);

    tests.push_back(t);
  }
  {
    tmp t(half - 1, half + 1, halfe.ByteSizeLong() * 2, false);
    Entry entry;

    entry.set_index(half - 1);
    entry.set_term(half - 1);
    t.entries.push_back(entry);

    entry.set_index(half);
    entry.set_term(half);
    t.entries.push_back(entry);
    tests.push_back(t);
  }
  {
    tmp t(half - 1, half + 2, halfe.ByteSizeLong() * 3, false);
    Entry entry;

    entry.set_index(half - 1);
    entry.set_term(half - 1);
    t.entries.push_back(entry);

    entry.set_index(half);
    entry.set_term(half);
    t.entries.push_back(entry);

    entry.set_index(half + 1);
    entry.set_term(half + 1);
    t.entries.push_back(entry);

    tests.push_back(t);
  }
  {
    tmp t(half, half + 2, halfe.ByteSizeLong(), false);
    Entry entry;

    entry.set_index(half);
    entry.set_term(half);
    t.entries.push_back(entry);
    tests.push_back(t);
  }
  {
    tmp t(half, half + 2, halfe.ByteSizeLong() * 2, false);
    Entry entry;

    entry.set_index(half);
    entry.set_term(half);
    t.entries.push_back(entry);

    entry.set_index(half + 1);
    entry.set_term(half + 1);
    t.entries.push_back(entry);
    tests.push_back(t);
  }

  for (i = 0; i < tests.size(); ++i) {
    const tmp &t = tests[i];
    EntryVec ents;

    int err = log->slice(t.from, t.to, t.limit, &ents);

    EXPECT_FALSE(t.from <= offset && err != ErrCompacted);
    EXPECT_FALSE(t.from > offset && !SUCCESS(err)) << "i: " << i;
    EXPECT_TRUE(isDeepEqualEntries(ents, t.entries)) << "i: " << i << ",size: " << ents.size();
  }

  delete log;
} 
