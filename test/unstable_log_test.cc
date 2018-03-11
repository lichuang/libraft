#include <gtest/gtest.h>
#include "libraft.h"
#include "unstable_log.h"

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

/*
int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
*/
