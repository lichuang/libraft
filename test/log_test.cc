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
  uint64_t lastindex = 3;
  uint64_t lastterm = 3;
  uint64_t commit = 1;

  // first fill up previous entries
  EntryVec previousEnts = {
    initEntry(1,1),
    initEntry(2,2),
    initEntry(3,3),
  };

  struct tmp {
    uint64_t logTerm;
    uint64_t index;
    uint64_t commited;
    EntryVec entries;

    uint64_t wlasti;
    bool wappend;
    uint64_t wcommit;
    bool wpanic;
  } tests[] = {
    // not match: term is different
    {
      .logTerm = lastterm - 1, .index = lastindex, .commited = lastindex,
      .entries = {initEntry(lastindex + 1, 4),},
      .wlasti = 0, .wappend = false, .wcommit = commit, .wpanic = false
    },
    // not match: index out of bound
    {
      .logTerm = lastterm, .index = lastindex + 1, .commited = lastindex,
      .entries = {initEntry(lastindex + 2, 4),},
      .wlasti = 0, .wappend = false, .wcommit = commit, .wpanic = false
    },
    // match with the last existing entry
    {
      .logTerm = lastterm, .index = lastindex, .commited = lastindex,
      .entries = {},
      .wlasti = lastindex, .wappend = true, .wcommit = lastindex, .wpanic = false
    },
    {
      // do not increase commit higher than lastnewi
      .logTerm = lastterm, .index = lastindex, .commited = lastindex + 1,
      .entries = {},
      .wlasti = lastindex, .wappend = true, .wcommit = lastindex, .wpanic = false
    },     
    {
      // commit up to the commit in the message
      .logTerm = lastterm, .index = lastindex, .commited = lastindex - 1,
      .entries = {},
      .wlasti = lastindex, .wappend = true, .wcommit = lastindex - 1, .wpanic = false
    },
    {
      // commit do not decrease
      .logTerm = lastterm, .index = lastindex, .commited = 0,
      .entries = {},
      .wlasti = lastindex, .wappend = true, .wcommit = commit, .wpanic = false
    },
    {
      // commit do not decrease
      .logTerm = 0, .index = 0, .commited = lastindex,
      .entries = {},
      .wlasti = 0, .wappend = true, .wcommit = commit, .wpanic = false
    },
    {
      .logTerm = lastterm, .index = lastindex, .commited = lastindex,
      .entries = {initEntry(lastindex + 1, 4)},
      .wlasti = lastindex + 1, .wappend = true, .wcommit = lastindex, .wpanic = false
    },
    {
      // do not increase commit higher than lastnewi
      .logTerm = lastterm, .index = lastindex, .commited = lastindex + 2,
      .entries = {initEntry(lastindex + 1, 4)},
      .wlasti = lastindex + 1, .wappend = true, .wcommit = lastindex + 1, .wpanic = false
    },
    {
      // do not increase commit higher than lastnewi
      .logTerm = lastterm, .index = lastindex, .commited = lastindex + 2,
      .entries = {initEntry(lastindex + 1, 4), initEntry(lastindex + 2, 4)},
      .wlasti = lastindex + 2, .wappend = true, .wcommit = lastindex + 2, .wpanic = false
    },    
    // match with the the entry in the middle
    {
      .logTerm = lastterm - 1, .index = lastindex - 1, .commited = lastindex,
      .entries = {initEntry(lastindex, 4)},
      .wlasti = lastindex, .wappend = true, .wcommit = lastindex, .wpanic = false
    },  
    {
      .logTerm = lastterm - 2, .index = lastindex - 2, .commited = lastindex,
      .entries = {initEntry(lastindex - 1, 4)},
      .wlasti = lastindex - 1, .wappend = true, .wcommit = lastindex - 1, .wpanic = false
    }, 
    /* this case will FATAL the lib
    // conflict with existing committed entry
    {
      .logTerm = lastterm - 3, .index = lastindex - 3, .commited = lastindex,
      .entries = {initEntry(lastindex - 1, 4)},
      .wlasti = lastindex - 2, .wappend = true, .wcommit = lastindex - 2, .wpanic = true
    },
    */  
    {
      .logTerm = lastterm - 2, .index = lastindex - 2, .commited = lastindex,
      .entries = {initEntry(lastindex - 1, 4), initEntry(lastindex, 4)},
      .wlasti = lastindex, .wappend = true, .wcommit = lastindex, .wpanic = false
    },                  
  };

  size_t i = 0;
  for (i = 0; i < SIZEOF_ARRAY(tests); ++i) {
    const tmp &test = tests[i];
    MemoryStorage s(&kDefaultLogger);
    raftLog *log = newLog(&s, &kDefaultLogger);
    log->append(previousEnts);
    log->committed_ = commit;

    uint64_t glasti;
    bool ok  = log->maybeAppend(test.index, test.logTerm, test.commited, test.entries, &glasti);
    uint64_t gcommit = log->committed_;

    EXPECT_EQ(glasti, test.wlasti);
    EXPECT_EQ(ok, test.wappend) << "i: " << i;
    EXPECT_EQ(gcommit, test.wcommit) << "i: " << i;
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
    s.Append({ 
      initEntry(i,i) 
    });
  }

  raftLog *log = newLog(&s, &kDefaultLogger);
  for (i = unstableIndex; i < lastIndex; ++i) {
    log->append({ 
      initEntry(i+1,i+1) 
    });
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
  log->append({ 
    initEntry(log->lastIndex() + 1, log->lastIndex() + 1) 
  });

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

  EntryVec entries = {
    initEntry(4,1),
    initEntry(5,1),
    initEntry(6,1),
  };

  struct tmp {
    uint64_t applied;
    bool hasNext;
  } tests[] = {
    { .applied = 0, .hasNext = true },
    { .applied = 3, .hasNext = true },
    { .applied = 4, .hasNext = true },
    { .applied = 5, .hasNext = false },
  };

  size_t i;
  for (i = 0; i < SIZEOF_ARRAY(tests); ++i) {
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

  EntryVec entries = {
    initEntry(4,1),
    initEntry(5,1),
    initEntry(6,1),
  };

  struct tmp {
    uint64_t applied;
    EntryVec entries;
  } tests[] = {
    {.applied = 0, .entries = EntryVec(entries.begin(), entries.begin() + 2)},
    {.applied = 3, .entries = EntryVec(entries.begin(), entries.begin() + 2)},
    {.applied = 4, .entries = EntryVec(entries.begin() + 1, entries.begin() + 2)},
    {.applied = 5, .entries = {}},
  };

  size_t i;
  for (i = 0; i < SIZEOF_ARRAY(tests); ++i) {
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
  EntryVec previousEnts = {
    initEntry(1, 1),
    initEntry(2, 2),
  };

  struct tmp {
    uint64_t unstable;
    EntryVec entries;
  } tests[] = {
    {.unstable = 3, .entries = {}},
    {.unstable = 1, .entries = EntryVec(previousEnts)},
  };

  size_t i = 0;
  for (i = 0; i < SIZEOF_ARRAY(tests); ++i) {
    const tmp &t = tests[i];

    // append stable entries to storage
    MemoryStorage s(&kDefaultLogger);
    s.Append(EntryVec(previousEnts.begin(), previousEnts.begin() + t.unstable - 1));    

    // append unstable entries to raftlog
    raftLog *log = newLog(&s, &kDefaultLogger); 
    log->append(EntryVec(previousEnts.begin() + t.unstable - 1, previousEnts.end()));

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
  uint64_t commit = 2;
  EntryVec previousEnts = {
    initEntry(1,1),
    initEntry(2,2),
    initEntry(3,3),
  };

  struct tmp {
    uint64_t commit, wcommit;
    bool panic;
  } tests[] = {
    {.commit = 3, .wcommit = 3, .panic = false},
    {.commit = 1, .wcommit = 2, .panic = false},  // never decrease
    //{.commit = 4, .wcommit = 0, .panic = true},   // commit out of range -> panic
  };

  size_t i;
  for (i = 0; i < SIZEOF_ARRAY(tests); ++i) {
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
  EntryVec entries = {
    initEntry(1,1),
    initEntry(2,2),
  };

  struct tmp {
    uint64_t stablei, stablet, wunstable;
  } tests[] = {
    {.stablei = 1, .stablet = 1, .wunstable = 2},
    {.stablei = 2, .stablet = 2, .wunstable = 3},
    {.stablei = 2, .stablet = 1, .wunstable = 1}, // bad term
    {.stablei = 3, .stablet = 1, .wunstable = 1}, // bad index
  };

  size_t i;
  for (i = 0; i < SIZEOF_ARRAY(tests); ++i) {
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
  } tests[] = {
    {
      .stablei = snapi + 1, .stablet = snapt, .wunstable = snapi + 1,
      .entries = {},
    },
    {
      .stablei = snapi, .stablet = snapt + 1, .wunstable = snapi + 1,
      .entries = {},
    },
    {
      .stablei = snapi - 1, .stablet = snapt, .wunstable = snapi + 1,
      .entries = {},
    },  
    {
      .stablei = snapi + 1, .stablet = snapt + 1, .wunstable = snapi + 1,
      .entries = {},
    }, 
    {
      .stablei = snapi, .stablet = snapt + 1, .wunstable = snapi + 1,
      .entries = {},
    },           
    {
      .stablei = snapi - 1, .stablet = snapt + 1, .wunstable = snapi + 1,
      .entries = {},
    },     
    {
      .stablei = snapi + 1, .stablet = snapt, .wunstable = snapi + 2,
      .entries = {initEntry(snapi + 1, snapt)},
    },      
    {
      .stablei = snapi, .stablet = snapt, .wunstable = snapi + 1,
      .entries = {initEntry(snapi + 1, snapt)},
    },     
    {
      .stablei = snapi - 1, .stablet = snapt, .wunstable = snapi + 1,
      .entries = {initEntry(snapi + 1, snapt)},
    },   
    {
      .stablei = snapi + 1, .stablet = snapt + 1, .wunstable = snapi + 1,
      .entries = {initEntry(snapi + 1, snapt)},
    },       
    {
      .stablei = snapi, .stablet = snapt + 1, .wunstable = snapi + 1,
      .entries = {initEntry(snapi + 1, snapt)},
    },    
    {
      .stablei = snapi - 1, .stablet = snapt + 1, .wunstable = snapi + 1,
      .entries = {initEntry(snapi + 1, snapt)},
    },     
  };

  size_t i = 0;
  for (i = 0; i < SIZEOF_ARRAY(tests); ++i) {
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
  } tests[] = {
    // out of upper bound
    /* this case will fatal the library
    {
      .lastIndex  = 1000,
      .compact    = {1001,},
      .wleft      = {-1},
      .wallow     = false,
    },
    */
    {
      .lastIndex  = 1000,
      .compact    = {300,500,800,900},
      .wleft      = {700,500,200,100},
      .wallow     = true,
    },
    // out of lower bound 
    {
      .lastIndex  = 1000,
      .compact    = {300,299},
      .wleft      = {700,-1},
      .wallow     = false,
    },    
  };

  size_t i = 0, j = 0;
  for (i = 0; i < SIZEOF_ARRAY(tests); ++i) {
    const tmp &t = tests[i];

    MemoryStorage s(&kDefaultLogger);
    EntryVec entries;
    for (j = 1; j <= t.lastIndex; ++j) {
      entries.push_back(initEntry(j));
    }
    s.Append(entries);

    raftLog *log = newLog(&s, &kDefaultLogger);
    log->maybeCommit(t.lastIndex, 0);
    log->appliedTo(log->committed_);

    for (j = 0; j < t.compact.size(); ++j) {
      int err = s.Compact(t.compact[j]);
      if (!SUCCESS(err)) {
        EXPECT_FALSE(t.wallow) << i << "." << j << " allow = " << false << ",want " << t.wallow;
        continue;
      }
      EntryVec allEntries;
      log->allEntries(&allEntries);
      EXPECT_EQ(t.wleft[j], (int)allEntries.size());
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
    entries.push_back(initEntry(i+offset));
  }
  log->append(entries);

  uint64_t first = offset + 1;

  struct tmp {
    uint64_t lo, hi;
    bool wpanic;
    bool errCompacted;
  } tests[] = {
    {
      .lo = first - 2, .hi = first + 1,
      .wpanic = false, .errCompacted = true,
    },
    {
      .lo = first - 1, .hi = first + 1,
      .wpanic = false, .errCompacted = true,
    },   
    {
      .lo = first, .hi = first,
      .wpanic = false, .errCompacted = false,
    },
    {
      .lo = first + num / 2, .hi = first + num / 2,
      .wpanic = false, .errCompacted = false,
    },
    {
      .lo = first + num - 1, .hi = first + num - 1,
      .wpanic = false, .errCompacted = false,
    },  
    {
      .lo = first + num, .hi = first + num,
      .wpanic = false, .errCompacted = false,
    },                
  };

  for (i = 0; i < SIZEOF_ARRAY(tests); ++i) {
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
    entries.push_back(initEntry(i + offset, i));
  }
  log->append(entries);

  struct tmp {
    uint64_t index, w;
  } tests[] = {
    {.index = offset - 1, .w = 0},
    {.index = offset,     .w = 1},
    {.index = offset + num / 2, .w = num / 2},
    {.index = offset + num - 1, .w = num - 1},
    {.index = offset + num, .w = 0},    
  };

  for (i = 0; i < SIZEOF_ARRAY(tests); ++i) {
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
  } tests[] = {
    // cannot get term from storage
    {.index = storagesnapi, .w = 0},
    // cannot get term from the gap between storage ents and unstable snapshot
    {.index = storagesnapi + 1, .w = 0},
    {.index = unstablesnapi - 1, .w = 0},
    // get term from unstable snapshot index
    {.index = unstablesnapi, .w = 1},
  };

  for (i = 0; i < SIZEOF_ARRAY(tests); ++i) {
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
    entries.push_back(initEntry(i + offset, i + offset));
  }
  log->append(entries);

  struct tmp {
    uint64_t from, to, limit;
    EntryVec entries;
    bool wpanic;
  } tests[] = {
    // test no limit
    {
      .from = offset - 1, .to = offset + 1, .limit = kNoLimit,
      .entries = {},
      .wpanic = false,      
    },
    {
      .from = offset, .to = offset + 1, .limit = kNoLimit,
      .entries = {},
      .wpanic = false,      
    },    
    {
      .from = half - 1, .to = half + 1, .limit = kNoLimit,
      .entries = {initEntry(half - 1, half - 1), initEntry(half, half)},
      .wpanic = false,      
    },    
    {
      .from = half, .to = half + 1, .limit = kNoLimit,
      .entries = {initEntry(half, half),},
      .wpanic = false,      
    },    
    {
      .from = last - 1, .to = last, .limit = kNoLimit,
      .entries = {initEntry(last - 1, last - 1),},
      .wpanic = false,      
    },  
    // test limit  
    {
      .from = half - 1, .to = half + 1, .limit = 0,
      .entries = {initEntry(half - 1, half - 1),},
      .wpanic = false,      
    },   
    {
      .from = half - 1, .to = half + 1, .limit = halfe.ByteSizeLong() + 1,
      .entries = {initEntry(half - 1, half - 1),},
      .wpanic = false,      
    },     
    {
      .from = half - 2, .to = half + 1, .limit = halfe.ByteSizeLong() + 1,
      .entries = {initEntry(half - 2, half - 2),},
      .wpanic = false,      
    },   
    {
      .from = half - 1, .to = half + 1, .limit = halfe.ByteSizeLong() * 2,
      .entries = {initEntry(half - 1, half - 1), initEntry(half, half),},
      .wpanic = false,      
    },     
    {
      .from = half - 1, .to = half + 2, .limit = halfe.ByteSizeLong() * 3,
      .entries = {initEntry(half - 1, half - 1), initEntry(half, half), initEntry(half + 1, half + 1),},
      .wpanic = false,      
    },      
    {
      .from = half, .to = half + 2, .limit = halfe.ByteSizeLong(),
      .entries = {initEntry(half, half),},
      .wpanic = false,      
    },   
    {
      .from = half, .to = half + 2, .limit = halfe.ByteSizeLong() * 2,
      .entries = {initEntry(half, half), initEntry(half + 1, half + 1),},
      .wpanic = false,      
    },       
  };

  for (i = 0; i < SIZEOF_ARRAY(tests); ++i) {
    const tmp &t = tests[i];
    EntryVec ents;

    int err = log->slice(t.from, t.to, t.limit, &ents);

    EXPECT_FALSE(t.from <= offset && err != ErrCompacted);
    EXPECT_FALSE(t.from > offset && !SUCCESS(err)) << "i: " << i;
    EXPECT_TRUE(isDeepEqualEntries(ents, t.entries)) << i << " from " << t.from << " to " << t.to;
  }

  delete log;
} 
