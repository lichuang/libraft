/*
 * Copyright (C) lichuang
 */

#include <gtest/gtest.h>
#include "libraft.h"
#include "raft_test_util.h"
#include "base/default_logger.h"
#include "base/util.h"
#include "storage/memory_storage.h"

using namespace libraft;

TEST(memoryStorageTests, TestStorageTerm) {
  EntryVec entries = {
    initEntry(3,3),
    initEntry(4,4),
    initEntry(5,5),
  };

  struct tmp {
    uint64_t i;
    int werr;
    uint64_t wterm;
  } tests[] = {
    {.i = 2, .werr = ErrCompacted, .wterm = 0},
    {.i = 3, .werr = OK, .wterm = 3},
    {.i = 4, .werr = OK, .wterm = 4},
    {.i = 5, .werr = OK, .wterm = 5},
    {.i = 6, .werr = ErrUnavailable, .wterm = 0},
  };
  size_t i = 0;
  for (i = 0; i < SIZEOF_ARRAY(tests); ++i) {
    const tmp &test = tests[i];
    MemoryStorage s(&kDefaultLogger);
    s.entries_ = entries;
    uint64_t term;
    int err = s.Term(test.i, &term);
    EXPECT_EQ(err, test.werr) << "i: " << i;
    EXPECT_EQ(term, test.wterm) << "i: " << i;
  }
}

TEST(memoryStorageTests, TestStorageEntries) {
  EntryVec entries = {
    initEntry(3,3),
    initEntry(4,4),
    initEntry(5,5),
    initEntry(6,6),
  };

  struct tmp {
    uint64_t lo, hi, maxsize;
    int werr;
    EntryVec entries;
  } tests[] = {
    {
      .lo = 2, .hi = 6, .maxsize = kNoLimit, .werr = ErrCompacted,
      .entries = {},
    },
    {
      .lo = 3, .hi = 4, .maxsize = kNoLimit, .werr = ErrCompacted,
      .entries = {},
    }, 
    {
      .lo = 4, .hi = 5, .maxsize = kNoLimit, .werr = OK,
      .entries = {initEntry(4,4)},
    },        
    {
      .lo = 4, .hi = 6, .maxsize = kNoLimit, .werr = OK,
      .entries = {initEntry(4,4), initEntry(5,5)},
    }, 
    {
      .lo = 4, .hi = 7, .maxsize = kNoLimit, .werr = OK,
      .entries = {initEntry(4,4), initEntry(5,5), initEntry(6,6)},
    },         
    // even if maxsize is zero, the first entry should be returned
    {
      .lo = 4, .hi = 7, .maxsize = 0, .werr = OK,
      .entries = {initEntry(4,4),},
    },   
    // limit to 2
    {
      .lo = 4, .hi = 7, .maxsize = entries[1].ByteSizeLong() + entries[2].ByteSizeLong(), .werr = OK,
      .entries = {initEntry(4,4),initEntry(5,5),},
    },     
    // limit to 2
    {
      .lo = 4, .hi = 7, .maxsize = entries[1].ByteSizeLong() + entries[2].ByteSizeLong() + entries[3].ByteSizeLong() / 2, .werr = OK,
      .entries = {initEntry(4,4),initEntry(5,5),},
    },   
    {
      .lo = 4, .hi = 7, .maxsize = entries[1].ByteSizeLong() + entries[2].ByteSizeLong() + entries[3].ByteSizeLong() - 1, .werr = OK,
      .entries = {initEntry(4,4),initEntry(5,5),},
    },  
    // all   
    {
      .lo = 4, .hi = 7, .maxsize = entries[1].ByteSizeLong() + entries[2].ByteSizeLong() + entries[3].ByteSizeLong(), .werr = OK,
      .entries = {initEntry(4,4),initEntry(5,5),initEntry(6,6)},
    },     
  };

  size_t i = 0;
  for (i = 0; i < SIZEOF_ARRAY(tests); ++i) {
    const tmp &test = tests[i];
    MemoryStorage s(&kDefaultLogger);
    EntryVec ret;
    s.entries_ = entries;

    int err = s.Entries(test.lo, test.hi, test.maxsize, &ret);
    EXPECT_EQ(err, test.werr) << "i: " << i;
    EXPECT_TRUE(isDeepEqualEntries(ret, test.entries)) << "i: " << i << ",ret:" << ret.size();
  }
}

TEST(memoryStorageTests, TestStorageLastIndex) {
  EntryVec entries = {
    initEntry(3,3),
    initEntry(4,4),
    initEntry(5,5),
  };

  MemoryStorage s(&kDefaultLogger);
  s.entries_ = entries;

  uint64_t last;
  int err = s.LastIndex(&last);
  EXPECT_EQ(OK, err);
  EXPECT_EQ((int)last, 5);

  s.Append(EntryVec({initEntry(6,5)}));

  err = s.LastIndex(&last);
  EXPECT_EQ(OK, err);
  EXPECT_EQ((int)last, 6);
}

TEST(memoryStorageTests, TestStorageFirstIndex) {
  EntryVec entries = {
    initEntry(3,3),
    initEntry(4,4),
    initEntry(5,5),
  };

  MemoryStorage s(&kDefaultLogger);
  s.entries_ = entries;

  {
    uint64_t first;
    int err = s.FirstIndex(&first);

    EXPECT_EQ(OK, err);
    EXPECT_EQ((int)first, 4);
  }

  s.Compact(4);

  {
    uint64_t first;
    int err = s.FirstIndex(&first);

    EXPECT_EQ(OK, err);
    EXPECT_EQ((int)first, 5);
  }
}

TEST(memoryStorageTests, TestStorageCompact) {
  EntryVec entries = {
    initEntry(3,3),
    initEntry(4,4),
    initEntry(5,5),
  };

  MemoryStorage s(&kDefaultLogger);
  s.entries_ = entries;

  struct tmp {
    uint64_t i;
    int werr;
    uint64_t wterm;
    uint64_t windex;
    int wlen;
  } tests[] = {
    { .i = 2, .werr = ErrCompacted, .wterm = 3, .windex = 3, .wlen = 3, },
    { .i = 3, .werr = ErrCompacted, .wterm = 3, .windex = 3, .wlen = 3, },
    { .i = 4, .werr = OK, .wterm = 4, .windex = 4, .wlen = 2, },
    { .i = 5, .werr = OK, .wterm = 5, .windex = 5, .wlen = 1, },
  };

  size_t i = 0;
  for (i = 0; i < SIZEOF_ARRAY(tests); ++i) {
    const tmp &test = tests[i];
    MemoryStorage tmp_s(&kDefaultLogger);
    tmp_s.entries_ = entries;
    
    int err = tmp_s.Compact(test.i);
    EXPECT_EQ(err, test.werr);
    EXPECT_EQ(tmp_s.entries_[0].index(), test.windex);
    EXPECT_EQ(tmp_s.entries_[0].term(), test.wterm);
    EXPECT_EQ((int)tmp_s.entries_.size(), test.wlen);
  }
}

//TODO:TestStorageCreateSnapshot
