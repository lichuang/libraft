#include <gtest/gtest.h>
#include "libraft.h"
#include "util.h"
#include "unstable_log.h"
#include "logger.h"

TEST(unstableLogTests, TestUnstableMaybeFirstIndex) {
  struct tmp {
    EntryVec entries;
    uint64_t offset;
    Snapshot *snapshot;
    bool wok;
    uint64_t windex;

    tmp(EntryVec ens, uint64_t off, Snapshot *snap, bool w, uint64_t index)
      : entries(ens), offset(off), snapshot(snap), wok(w), windex(index) {
    }
  };

  vector<tmp> tests;

  // no  snapshot
  {
    Entry entry;
    entry.set_index(5);
    entry.set_term(1);
    EntryVec entries;
    entries.push_back(entry);
    tmp t(entries, 5, NULL, false, 0);

    tests.push_back(t);
  }
  {
    EntryVec entries;
    tmp t(entries, 0, NULL, false, 0);

    tests.push_back(t);
  }
  // has snapshot
  {
    Entry entry;
    entry.set_index(5);
    entry.set_term(1);
    EntryVec entries;
    entries.push_back(entry);
    Snapshot *snapshot = new Snapshot();
    SnapshotMetadata *meta = snapshot->mutable_metadata();
    meta->set_index(4);
    meta->set_term(1);
    tmp t(entries, 5, snapshot, true, 5);

    tests.push_back(t);
  }
  {
    EntryVec entries;
    Snapshot *snapshot = new Snapshot();
    SnapshotMetadata *meta = snapshot->mutable_metadata();
    meta->set_index(4);
    meta->set_term(1);
    tmp t(entries, 5, snapshot, true, 5);

    tests.push_back(t);
  }

  int i;
  for (i = 0;i < tests.size(); ++i) {
    unstableLog unstable;
    unstable.entries_ = tests[i].entries;
    unstable.offset_  = tests[i].offset;
    unstable.snapshot_  = tests[i].snapshot;
    unstable.logger_  = NULL;

    uint64_t index = unstable.maybeFirstIndex();
    EXPECT_EQ(tests[i].wok, index != 0);
  }
}

TEST(unstableLogTests, TestMaybeLastIndex) {
  struct tmp {
    EntryVec entries;
    uint64_t offset;
    Snapshot *snapshot;
    bool wok;
    uint64_t windex;

    tmp(EntryVec ens, uint64_t off, Snapshot *snap, bool w, uint64_t index)
      : entries(ens), offset(off), snapshot(snap), wok(w), windex(index) {
    }
  };

  vector<tmp> tests;

  // last in entries
  {
    Entry entry;
    entry.set_index(5);
    entry.set_term(1);
    EntryVec entries;
    entries.push_back(entry);
    tmp t(entries, 5, NULL, true, 5);

    tests.push_back(t);
  }
  {
    Entry entry;
    entry.set_index(5);
    entry.set_term(1);
    EntryVec entries;
    entries.push_back(entry);
    Snapshot *snapshot = new Snapshot();
    SnapshotMetadata *meta = snapshot->mutable_metadata();
    meta->set_index(4);
    meta->set_term(1);
    tmp t(entries, 5, snapshot, true, 5);

    tests.push_back(t);
  }
  // last in entries
  {
    EntryVec entries;
    Snapshot *snapshot = new Snapshot();
    SnapshotMetadata *meta = snapshot->mutable_metadata();
    meta->set_index(4);
    meta->set_term(1);
    tmp t(entries, 5, snapshot, true, 4);

    tests.push_back(t);
  }
  // empty unstable
  {
    EntryVec entries;
    tmp t(entries, 0, NULL, false, 0);

    tests.push_back(t);
  }

  int i;
  for (i = 0;i < tests.size(); ++i) {
    unstableLog unstable;
    unstable.entries_ = tests[i].entries;
    unstable.offset_  = tests[i].offset;
    unstable.snapshot_  = tests[i].snapshot;
    unstable.logger_  = NULL;

    uint64_t index = unstable.maybeLastIndex();
    EXPECT_EQ(tests[i].wok, index != 0);
  }
}

TEST(unstableLogTests, TestUnstableMaybeTerm) {
  struct tmp {
    EntryVec entries;
    uint64_t offset;
    Snapshot *snapshot;
    uint64_t index;
    bool wok;
    uint64_t wterm;

    tmp(EntryVec ens, uint64_t off, Snapshot *snap, uint64_t i, bool w, uint64_t t)
      : entries(ens), offset(off), snapshot(snap), index(i), wok(w), wterm(t) {
    }
  };

  vector<tmp> tests;
  // term from entries
  {
    Entry entry;
    entry.set_index(5);
    entry.set_term(1);
    EntryVec entries;
    entries.push_back(entry);
    tmp t(entries, 5, NULL, 5, true, 1);

    tests.push_back(t);
  }
  {
    Entry entry;
    entry.set_index(5);
    entry.set_term(1);
    EntryVec entries;
    entries.push_back(entry);
    tmp t(entries, 5, NULL, 6, false, 0);

    tests.push_back(t);
  }
  {
    Entry entry;
    entry.set_index(5);
    entry.set_term(1);
    EntryVec entries;
    entries.push_back(entry);
    tmp t(entries, 5, NULL, 4, false, 0);

    tests.push_back(t);
  }
  {
    Entry entry;
    entry.set_index(5);
    entry.set_term(1);
    EntryVec entries;
    entries.push_back(entry);
    Snapshot *snapshot = new Snapshot();
    SnapshotMetadata *meta = snapshot->mutable_metadata();
    meta->set_index(4);
    meta->set_term(1);
    tmp t(entries, 5, snapshot, 5, true, 1);

    tests.push_back(t);
  }
  {
    Entry entry;
    entry.set_index(5);
    entry.set_term(1);
    EntryVec entries;
    entries.push_back(entry);
    Snapshot *snapshot = new Snapshot();
    SnapshotMetadata *meta = snapshot->mutable_metadata();
    meta->set_index(4);
    meta->set_term(1);
    tmp t(entries, 5, snapshot, 6, false, 0);

    tests.push_back(t);
  }
  // term from snapshot
  {
    Entry entry;
    entry.set_index(5);
    entry.set_term(1);
    EntryVec entries;
    entries.push_back(entry);
    Snapshot *snapshot = new Snapshot();
    SnapshotMetadata *meta = snapshot->mutable_metadata();
    meta->set_index(4);
    meta->set_term(1);
    tmp t(entries, 5, snapshot, 4, true, 1);

    tests.push_back(t);
  }
  {
    Entry entry;
    entry.set_index(5);
    entry.set_term(1);
    EntryVec entries;
    entries.push_back(entry);
    Snapshot *snapshot = new Snapshot();
    SnapshotMetadata *meta = snapshot->mutable_metadata();
    meta->set_index(4);
    meta->set_term(1);
    tmp t(entries, 5, snapshot, 3, false, 0);

    tests.push_back(t);
  }
  {
    EntryVec entries;
    Snapshot *snapshot = new Snapshot();
    SnapshotMetadata *meta = snapshot->mutable_metadata();
    meta->set_index(4);
    meta->set_term(1);
    tmp t(entries, 5, snapshot, 5, false, 0);

    tests.push_back(t);
  }
  {
    EntryVec entries;
    Snapshot *snapshot = new Snapshot();
    SnapshotMetadata *meta = snapshot->mutable_metadata();
    meta->set_index(4);
    meta->set_term(1);
    tmp t(entries, 5, snapshot, 4, true, 1);

    tests.push_back(t);
  }
  {
    EntryVec entries;
    tmp t(entries, 0, NULL, 5, false, 0);

    tests.push_back(t);
  }

  int i;
  for (i = 0;i < tests.size(); ++i) {
    unstableLog unstable;
    unstable.entries_ = tests[i].entries;
    unstable.offset_  = tests[i].offset;
    unstable.snapshot_  = tests[i].snapshot;
    unstable.logger_  = NULL;

    uint64_t term = unstable.maybeTerm(tests[i].index);
    EXPECT_EQ(tests[i].wok, term != 0) << "i: " << i << ", index: " << tests[i].index;
    EXPECT_EQ(tests[i].wterm, term);
  }
}

TEST(unstableLogTests, TestUnstableRestore) {
  Snapshot *snapshot = new Snapshot();
  SnapshotMetadata *meta = snapshot->mutable_metadata();
  meta->set_index(4);
  meta->set_term(1);
  Entry entry;
  entry.set_index(5);
  entry.set_term(1);
  EntryVec entries;
  entries.push_back(entry);

  unstableLog unstable;
  unstable.entries_ = entries;
  unstable.offset_  = 5;
  unstable.snapshot_  = snapshot;
  unstable.logger_  = NULL;

  Snapshot *s = new Snapshot();
  {
    SnapshotMetadata *meta = s->mutable_metadata();
    meta->set_index(6);
    meta->set_term(2);
    unstable.restore(s);
  }

  EXPECT_EQ(unstable.offset_, s->metadata().index() + 1);
  EXPECT_EQ(unstable.entries_.size(), 0);
  EXPECT_EQ(true, isDeepEqualSnapshot(unstable.snapshot_, s));
}

TEST(unstableLogTests, TestUnstableStableTo) {
  struct tmp {
    EntryVec entries;
    uint64_t offset;
    Snapshot *snapshot;
    uint64_t index, term;

    uint64_t woffset;
    int wlen;

    tmp(EntryVec ens, uint64_t off, Snapshot *snap, uint64_t index, uint64_t term, uint64_t wo, int wl)
      : entries(ens), offset(off), snapshot(snap), index(index), term(term), woffset(wo), wlen(wl) {
    }
  };

  vector<tmp> tests;
  {
    EntryVec entries;
    tmp t(entries, 0, NULL, 5, 1, 0, 0);

    tests.push_back(t);
  }
  {
    Entry entry;
    entry.set_index(5);
    entry.set_term(1);
    EntryVec entries;
    entries.push_back(entry); 
    // stable to the first entry
    tmp t(entries, 5, NULL, 5, 1, 6, 0);

    tests.push_back(t);
  }
  {
    Entry entry;
    entry.set_index(5);
    entry.set_term(1);
    EntryVec entries;
    entries.push_back(entry); 
    entry.set_index(6);
    entry.set_term(1);
    entries.push_back(entry); 
    // stable to the first entry
    tmp t(entries, 5, NULL, 5, 1, 6, 1);

    tests.push_back(t);
  }
  {
    Entry entry;
    entry.set_index(6);
    entry.set_term(2);
    EntryVec entries;
    entries.push_back(entry); 
    // stable to the first entry and term mismatch
    tmp t(entries, 6, NULL, 6, 1, 6, 1);

    tests.push_back(t);
  }
  {
    Entry entry;
    entry.set_index(5);
    entry.set_term(1);
    EntryVec entries;
    entries.push_back(entry); 
    // stable to old entry
    tmp t(entries, 5, NULL, 4, 1, 5, 1);

    tests.push_back(t);
  }
  {
    Entry entry;
    entry.set_index(5);
    entry.set_term(1);
    EntryVec entries;
    entries.push_back(entry); 
    // stable to old entry
    tmp t(entries, 5, NULL, 4, 2, 5, 1);

    tests.push_back(t);
  }
  // with snapshot
  {
    Entry entry;
    entry.set_index(5);
    entry.set_term(1);
    EntryVec entries;
    entries.push_back(entry); 
    Snapshot *snapshot = new Snapshot();
    SnapshotMetadata *meta = snapshot->mutable_metadata();
    meta->set_index(4);
    meta->set_term(1);
    // stable to the first entry
    tmp t(entries, 5, snapshot, 5, 1, 6, 0);

    tests.push_back(t);
  }
  {
    Entry entry;
    entry.set_index(5);
    entry.set_term(1);
    EntryVec entries;
    entries.push_back(entry); 
    entry.set_index(6);
    entry.set_term(1);
    entries.push_back(entry); 
    Snapshot *snapshot = new Snapshot();
    SnapshotMetadata *meta = snapshot->mutable_metadata();
    meta->set_index(4);
    meta->set_term(1);
    // stable to the first entry
    tmp t(entries, 5, snapshot, 5, 1, 6, 1);

    tests.push_back(t);
  }
  {
    Entry entry;
    entry.set_index(6);
    entry.set_term(2);
    EntryVec entries;
    entries.push_back(entry); 
    Snapshot *snapshot = new Snapshot();
    SnapshotMetadata *meta = snapshot->mutable_metadata();
    meta->set_index(5);
    meta->set_term(1);
    // stable to the first entry and term mismatch
    tmp t(entries, 6, snapshot, 6, 1, 6, 1);

    tests.push_back(t);
  }
  {
    Entry entry;
    entry.set_index(5);
    entry.set_term(1);
    EntryVec entries;
    entries.push_back(entry); 
    Snapshot *snapshot = new Snapshot();
    SnapshotMetadata *meta = snapshot->mutable_metadata();
    meta->set_index(5);
    meta->set_term(1);
    // stable to snapshot
    tmp t(entries, 5, snapshot, 4, 1, 5, 1);

    tests.push_back(t);
  }
  {
    Entry entry;
    entry.set_index(5);
    entry.set_term(2);
    EntryVec entries;
    entries.push_back(entry); 
    Snapshot *snapshot = new Snapshot();
    SnapshotMetadata *meta = snapshot->mutable_metadata();
    meta->set_index(4);
    meta->set_term(2);
    // stable to snapshot
    tmp t(entries, 5, snapshot, 4, 1, 5, 1);

    tests.push_back(t);
  }
  int i;
  for (i = 0;i < tests.size(); ++i) {
    unstableLog unstable;
    unstable.entries_ = tests[i].entries;
    unstable.offset_  = tests[i].offset;
    unstable.snapshot_  = tests[i].snapshot;
    unstable.logger_  = NULL;

    unstable.stableTo(tests[i].index, tests[i].term);
    EXPECT_EQ(unstable.offset_, tests[i].woffset) << "i: " << i << ", woffset: " << tests[i].woffset;
    EXPECT_EQ(unstable.entries_.size(), tests[i].wlen);
  }
}

TEST(unstableLogTests, TestUnstableTruncateAndAppend) {
  struct tmp {
    EntryVec entries;
    uint64_t offset;
    Snapshot *snapshot;
    EntryVec toappend;
    uint64_t woffset;
    EntryVec wentries;

    tmp(EntryVec ens, uint64_t off, Snapshot *snap, EntryVec append, uint64_t wo, EntryVec wents)
      : entries(ens), offset(off), snapshot(snap), toappend(append), woffset(wo), wentries(wents) {
    }
  };

  vector<tmp> tests;
  // append to the end
  {
    Entry entry;
    EntryVec entries, append, wents;
    
    entry.set_index(5);
    entry.set_term(1);
    entries.push_back(entry); 
    
    entry.set_index(6);
    entry.set_term(1);
    append.push_back(entry);
    entry.set_index(7);
    entry.set_term(1);
    append.push_back(entry);

    entry.set_index(5);
    entry.set_term(1);
    wents.push_back(entry);
    entry.set_index(6);
    entry.set_term(1);
    wents.push_back(entry);
    entry.set_index(7);
    entry.set_term(1);
    wents.push_back(entry);
    tmp t(entries, 5, NULL, append, 5, wents);

    tests.push_back(t);
  }
  // replace the unstable entries
  {
    Entry entry;
    EntryVec entries, append, wents;
    
    entry.set_index(5);
    entry.set_term(1);
    entries.push_back(entry); 
    
    entry.set_index(5);
    entry.set_term(2);
    append.push_back(entry);
    entry.set_index(6);
    entry.set_term(2);
    append.push_back(entry);

    entry.set_index(5);
    entry.set_term(2);
    wents.push_back(entry);
    entry.set_index(6);
    entry.set_term(2);
    wents.push_back(entry);

    tmp t(entries, 5, NULL, append, 5, wents);

    tests.push_back(t);
  }
  {
    Entry entry;
    EntryVec entries, append, wents;
    
    entry.set_index(5);
    entry.set_term(1);
    entries.push_back(entry); 
    
    entry.set_index(4);
    entry.set_term(2);
    append.push_back(entry);
    entry.set_index(5);
    entry.set_term(2);
    append.push_back(entry);
    entry.set_index(6);
    entry.set_term(2);
    append.push_back(entry);

    entry.set_index(4);
    entry.set_term(2);
    wents.push_back(entry);
    entry.set_index(5);
    entry.set_term(2);
    wents.push_back(entry);
    entry.set_index(6);
    entry.set_term(2);
    wents.push_back(entry);

    tmp t(entries, 5, NULL, append, 4, wents);

    tests.push_back(t);
  }
  // truncate the existing entries and append
  {
    Entry entry;
    EntryVec entries, append, wents;
    
    entry.set_index(5);
    entry.set_term(1);
    entries.push_back(entry); 
    entry.set_index(6);
    entry.set_term(1);
    entries.push_back(entry); 
    entry.set_index(7);
    entry.set_term(1);
    entries.push_back(entry); 
    
    entry.set_index(6);
    entry.set_term(2);
    append.push_back(entry);

    entry.set_index(5);
    entry.set_term(1);
    wents.push_back(entry);
    entry.set_index(6);
    entry.set_term(2);
    wents.push_back(entry);

    tmp t(entries, 5, NULL, append, 5, wents);

    tests.push_back(t);
  }
  {
    Entry entry;
    EntryVec entries, append, wents;
    
    entry.set_index(5);
    entry.set_term(1);
    entries.push_back(entry); 
    entry.set_index(6);
    entry.set_term(1);
    entries.push_back(entry); 
    entry.set_index(7);
    entry.set_term(1);
    entries.push_back(entry); 
    
    entry.set_index(7);
    entry.set_term(2);
    append.push_back(entry);
    entry.set_index(8);
    entry.set_term(2);
    append.push_back(entry);

    entry.set_index(5);
    entry.set_term(1);
    wents.push_back(entry);
    entry.set_index(6);
    entry.set_term(1);
    wents.push_back(entry);
    entry.set_index(7);
    entry.set_term(2);
    wents.push_back(entry);
    entry.set_index(8);
    entry.set_term(2);
    wents.push_back(entry);

    tmp t(entries, 5, NULL, append, 5, wents);

    tests.push_back(t);
  }

  int i;
  for (i = 0;i < tests.size(); ++i) {
    unstableLog unstable;
    unstable.entries_ = tests[i].entries;
    unstable.offset_  = tests[i].offset;
    unstable.snapshot_  = tests[i].snapshot;
    unstable.logger_  = &kDefaultLogger;

    unstable.truncateAndAppend(tests[i].toappend);
    EXPECT_EQ(unstable.offset_, tests[i].woffset) << "i: " << i << ", woffset: " << tests[i].woffset;
    EXPECT_EQ(true, isDeepEqualEntries(unstable.entries_, tests[i].wentries)) << "i: " << i;
  }
}
