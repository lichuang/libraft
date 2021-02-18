/*
 * Copyright (C) lichuang
 */

#include <gtest/gtest.h>
#include "libraft.h"
#include "base/default_logger.h"
#include "base/util.h"
#include "storage/memory_storage.h"

using namespace libraft;

TEST(memoryStorageTests, TestStorageTerm) {
  EntryVec entries;

  {
    Entry entry;

    entry.set_index(3);
    entry.set_term(3);
    entries.push_back(entry);

    entry.set_index(4);
    entry.set_term(4);
    entries.push_back(entry);

    entry.set_index(5);
    entry.set_term(5);
    entries.push_back(entry);
  }

  struct tmp {
    uint64_t i;
    int werr;
    uint64_t wterm;

    tmp(uint64_t i, int err, uint64_t term)
      : i(i), werr(err), wterm(term) {}
  };
  vector<tmp> tests;
  tests.push_back(tmp(2, ErrCompacted, 0));
  tests.push_back(tmp(3, OK, 3));
  tests.push_back(tmp(4, OK, 4));
  tests.push_back(tmp(5, OK, 5));
  tests.push_back(tmp(6, ErrUnavailable, 0));
  size_t i = 0;
  for (i = 0; i < tests.size(); ++i) {
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
  EntryVec entries;

  {
    Entry entry;

    entry.set_index(3);
    entry.set_term(3);
    entries.push_back(entry);

    entry.set_index(4);
    entry.set_term(4);
    entries.push_back(entry);

    entry.set_index(5);
    entry.set_term(5);
    entries.push_back(entry);

    entry.set_index(6);
    entry.set_term(6);
    entries.push_back(entry);
  }

  struct tmp {
    uint64_t lo, hi, maxsize;
    int werr;
    EntryVec entries;

    tmp(uint64_t lo, uint64_t hi, uint64_t maxsize, int err)
      : lo(lo), hi(hi), maxsize(maxsize), werr(err) {}
  };

  vector<tmp> tests;
  {
    tests.push_back(tmp(2, 6, kNoLimit, ErrCompacted));
    tests.push_back(tmp(3, 4, kNoLimit, ErrCompacted));

    {
      Entry entry;

      tmp t(4, 5, kNoLimit, OK);

      entry.set_index(4);
      entry.set_term(4);
      t.entries.push_back(entry);
      tests.push_back(t); 
    }

    {
      Entry entry;

      tmp t(4, 6, kNoLimit, OK);

      entry.set_index(4);
      entry.set_term(4);
      t.entries.push_back(entry);

      entry.set_index(5);
      entry.set_term(5);
      t.entries.push_back(entry);

      tests.push_back(t); 
    }

    {
      Entry entry;

      tmp t(4, 7, kNoLimit, OK);

      entry.set_index(4);
      entry.set_term(4);
      t.entries.push_back(entry);

      entry.set_index(5);
      entry.set_term(5);
      t.entries.push_back(entry);

      entry.set_index(6);
      entry.set_term(6);
      t.entries.push_back(entry);
      tests.push_back(t); 
    }
    // even if maxsize is zero, the first entry should be returned
    {
      Entry entry;

      tmp t(4, 7, 0, OK);

      entry.set_index(4);
      entry.set_term(4);
      t.entries.push_back(entry);

      tests.push_back(t); 
    }
    // limit to 2
    {
      Entry entry;

      int size = entries[1].ByteSizeLong() + entries[2].ByteSizeLong();
      tmp t(4, 7, size, OK);

      entry.set_index(4);
      entry.set_term(4);
      t.entries.push_back(entry);

      entry.set_index(5);
      entry.set_term(5);
      t.entries.push_back(entry);

      tests.push_back(t); 
    }
    // limit to 2
    {
      Entry entry;

      int size = entries[1].ByteSizeLong() + entries[2].ByteSizeLong() + entries[3].ByteSizeLong() / 2;
      tmp t(4, 7, size, OK);

      entry.set_index(4);
      entry.set_term(4);
      t.entries.push_back(entry);

      entry.set_index(5);
      entry.set_term(5);
      t.entries.push_back(entry);

      tests.push_back(t); 
    }
    {
      Entry entry;

      int size = entries[1].ByteSizeLong() + entries[2].ByteSizeLong() + entries[3].ByteSizeLong() - 1;
      tmp t(4, 7, size, OK);

      entry.set_index(4);
      entry.set_term(4);
      t.entries.push_back(entry);

      entry.set_index(5);
      entry.set_term(5);
      t.entries.push_back(entry);

      tests.push_back(t); 
    }
    // all
    {
      Entry entry;

      int size = entries[1].ByteSizeLong() + entries[2].ByteSizeLong() + entries[3].ByteSizeLong();
      tmp t(4, 7, size, OK);

      entry.set_index(4);
      entry.set_term(4);
      t.entries.push_back(entry);

      entry.set_index(5);
      entry.set_term(5);
      t.entries.push_back(entry);

      entry.set_index(6);
      entry.set_term(6);
      t.entries.push_back(entry);

      tests.push_back(t); 
    }
  }
  size_t i = 0;
  for (i = 0; i < tests.size(); ++i) {
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
  EntryVec entries;

  {
    Entry entry;

    entry.set_index(3);
    entry.set_term(3);
    entries.push_back(entry);

    entry.set_index(4);
    entry.set_term(4);
    entries.push_back(entry);

    entry.set_index(5);
    entry.set_term(5);
    entries.push_back(entry);
  }

  MemoryStorage s(&kDefaultLogger);
  s.entries_ = entries;

  uint64_t last;
  int err = s.LastIndex(&last);
  EXPECT_EQ(OK, err);
  EXPECT_EQ((int)last, 5);

  {
    EntryVec tmp_entries;
    Entry entry;

    entry.set_index(6);
    entry.set_term(5);
    tmp_entries.push_back(entry);
    s.Append(tmp_entries);
  }

  err = s.LastIndex(&last);
  EXPECT_EQ(OK, err);
  EXPECT_EQ((int)last, 6);
}

TEST(memoryStorageTests, TestStorageFirstIndex) {
  EntryVec entries;

  {
    Entry entry;

    entry.set_index(3);
    entry.set_term(3);
    entries.push_back(entry);

    entry.set_index(4);
    entry.set_term(4);
    entries.push_back(entry);

    entry.set_index(5);
    entry.set_term(5);
    entries.push_back(entry);
  }

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
  EntryVec entries;

  {
    Entry entry;

    entry.set_index(3);
    entry.set_term(3);
    entries.push_back(entry);

    entry.set_index(4);
    entry.set_term(4);
    entries.push_back(entry);

    entry.set_index(5);
    entry.set_term(5);
    entries.push_back(entry);
  }

  MemoryStorage s(&kDefaultLogger);
  s.entries_ = entries;

  struct tmp {
    uint64_t i;
    int werr;
    uint64_t wterm;
    uint64_t windex;
    int wlen;

    tmp(uint64_t i, int err, uint64_t index, uint64_t term, int len)
      : i(i), werr(err), wterm(term), windex(index), wlen(len) {}
  };
  vector<tmp> tests;
  tests.push_back(tmp(2, ErrCompacted, 3, 3, 3));
  tests.push_back(tmp(3, ErrCompacted, 3, 3, 3));
  tests.push_back(tmp(4, OK, 4, 4, 2));
  tests.push_back(tmp(5, OK, 5, 5, 1));
  size_t i = 0;
  for (i = 0; i < tests.size(); ++i) {
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
