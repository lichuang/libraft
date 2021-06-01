/*
 * Copyright (C) lichuang
 */

#include <gtest/gtest.h>
#include "libraft.h"
#include "raft_test_util.h"
#include "base/default_logger.h"
#include "base/util.h"
#include "storage/unstable_log.h"

using namespace libraft;

TEST(unstableLogTests, TestUnstableMaybeFirstIndex) {
  struct tmp {
    EntryVec entries;
    uint64_t offset;
    Snapshot *snapshot;
    bool wok;
    uint64_t windex;
  } tests[] = {
    // no  snapshot
    {
      .entries = {initEntry(5,1)},
      .offset = 5, .snapshot = NULL,
      .wok = false, .windex = 0,
    },
    {
      .entries = {},
      .offset = 0, .snapshot = NULL,
      .wok = false, .windex = 0,      
    },
    // has snapshot
    {
      .entries = {initEntry(5,1)},
      .offset = 5, .snapshot = newSnapshot(4,1),
      .wok = true, .windex = 5,
    },    
    {
      .entries = {},
      .offset = 5, .snapshot = newSnapshot(4,1),
      .wok = true, .windex = 5,
    },    
  };

  size_t i;
  for (i = 0;i < SIZEOF_ARRAY(tests); ++i) {
    unstableLog unstable;
    unstable.entries_ = tests[i].entries;
    unstable.offset_  = tests[i].offset;
    unstable.snapshot_  = tests[i].snapshot;
    unstable.logger_  = NULL;

    uint64_t index;
    bool ok = unstable.maybeFirstIndex(&index);
    EXPECT_EQ(tests[i].wok, ok);
    EXPECT_EQ(tests[i].windex, index);
    if (tests[i].snapshot != NULL) {
      delete tests[i].snapshot;
    }
  }
}

TEST(unstableLogTests, TestMaybeLastIndex) {
  struct tmp {
    EntryVec entries;
    uint64_t offset;
    Snapshot *snapshot;
    bool wok;
    uint64_t windex;
  } tests[] = {
    // last in entries
    {
      .entries = {initEntry(5,1),},
      .offset = 5, .snapshot = NULL,
      .wok = true, .windex = 5,
    },   
    {
      .entries = {initEntry(5,1),},
      .offset = 5, .snapshot = newSnapshot(4,1),
      .wok = true, .windex = 5,
    },  
    // last in snapshot     
    {
      .entries = {},
      .offset = 5, .snapshot = newSnapshot(4,1),
      .wok = true, .windex = 4,
    },   
    // empty unstable
    {
      .entries = {},
      .offset = 0, .snapshot = NULL,
      .wok = false, .windex = 0,
    },         
  };

  size_t i;
  for (i = 0;i < SIZEOF_ARRAY(tests); ++i) {
    unstableLog unstable;
    unstable.entries_ = tests[i].entries;
    unstable.offset_  = tests[i].offset;
    unstable.snapshot_  = tests[i].snapshot;
    unstable.logger_  = NULL;

    uint64_t index;
    bool ok = unstable.maybeLastIndex(&index);
    EXPECT_EQ(tests[i].wok, ok);
    EXPECT_EQ(tests[i].windex, index);

    if (tests[i].snapshot != NULL) {
      delete tests[i].snapshot;
    }    
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
  } tests[] = {
    // term from entries
    {
      .entries = {initEntry(5,1)},
      .offset = 5, .snapshot = NULL,
      .index = 5, .wok = true, .wterm = 1,
    },
    {
      .entries = {initEntry(5,1)},
      .offset = 5, .snapshot = NULL,
      .index = 6, .wok = false, .wterm = 0,
    },    
    {
      .entries = {initEntry(5,1)},
      .offset = 5, .snapshot = NULL,
      .index = 4, .wok = false, .wterm = 0,
    },      
    {
      .entries = {initEntry(5,1)},
      .offset = 5, .snapshot = newSnapshot(4,1),
      .index = 5, .wok = true, .wterm = 1,
    },  
    {
      .entries = {initEntry(5,1)},
      .offset = 5, .snapshot = newSnapshot(4,1),
      .index = 6, .wok = false, .wterm = 0,
    },  
    // term from snapshot  
    {
      .entries = {initEntry(5,1)},
      .offset = 5, .snapshot = newSnapshot(4,1),
      .index = 4, .wok = true, .wterm = 1,
    },
    {
      .entries = {initEntry(5,1)},
      .offset = 5, .snapshot = newSnapshot(4,1),
      .index = 3, .wok = false, .wterm = 0,
    },         
    {
      .entries = {},
      .offset = 5, .snapshot = newSnapshot(4,1),
      .index = 5, .wok = false, .wterm = 0,
    },      
    {
      .entries = {},
      .offset = 5, .snapshot = newSnapshot(4,1),
      .index = 4, .wok = true, .wterm = 1,
    }, 
    {
      .entries = {},
      .offset = 0, .snapshot = NULL,
      .index = 5, .wok = false, .wterm = 0,
    },               
  };

  size_t i;
  for (i = 0;i < SIZEOF_ARRAY(tests); ++i) {
    unstableLog unstable;
    unstable.entries_ = tests[i].entries;
    unstable.offset_  = tests[i].offset;
    unstable.snapshot_  = tests[i].snapshot;
    unstable.logger_  = NULL;

    uint64_t term;
    bool ok = unstable.maybeTerm(tests[i].index, &term);
    EXPECT_EQ(tests[i].wok, ok) << "i: " << i << ", index: " << tests[i].index;
    EXPECT_EQ(tests[i].wterm, term);

    if (tests[i].snapshot != NULL) {
      delete tests[i].snapshot;
    }      
  }
}

TEST(unstableLogTests, TestUnstableRestore) {
  unstableLog unstable;
  unstable.entries_ = {initEntry(5,1)};;
  unstable.offset_  = 5;
  unstable.snapshot_  = newSnapshot(4,1);
  unstable.logger_  = NULL;

  Snapshot s;
  {
    SnapshotMetadata *tmp_meta = s.mutable_metadata();
    tmp_meta->set_index(6);
    tmp_meta->set_term(2);
    unstable.restore(s);
  }

  EXPECT_EQ(unstable.offset_, s.metadata().index() + 1);
  EXPECT_EQ((int)unstable.entries_.size(), 0);
  EXPECT_TRUE(isDeepEqualSnapshot(unstable.snapshot_, &s));

  delete unstable.snapshot_;
}

TEST(unstableLogTests, TestUnstableStableTo) {
  struct tmp {
    EntryVec entries;
    uint64_t offset;
    Snapshot *snapshot;
    uint64_t index, term;

    uint64_t woffset;
    int wlen;

  } tests[] = {
    {
      .entries = {},
      .offset = 0, .snapshot = NULL,
      .index = 5, .term = 1,
      .woffset = 0, .wlen = 0,
    },    
    {
      .entries = {initEntry(5,1)},
      .offset = 5, .snapshot = NULL,
      .index = 5, .term = 1,  // stable to the first entry
      .woffset = 6, .wlen = 0,
    },    
    {
      .entries = {initEntry(5,1), initEntry(6,1)},
      .offset = 5, .snapshot = NULL,
      .index = 5, .term = 1,  // stable to the first entry
      .woffset = 6, .wlen = 1,
    },  
    {
      .entries = {initEntry(6,2)},
      .offset = 6, .snapshot = NULL,
      .index = 6, .term = 1,  // stable to the first entry and term mismatch
      .woffset = 6, .wlen = 1,
    },       
    {
      .entries = {initEntry(5,1)},
      .offset = 5, .snapshot = NULL,
      .index = 4, .term = 1,  // stable to old entry
      .woffset = 5, .wlen = 1,
    },       
    {
      .entries = {initEntry(5,1)},
      .offset = 5, .snapshot = NULL,
      .index = 4, .term = 2,  // stable to old entry
      .woffset = 5, .wlen = 1,
    },
    // with snapshot      
    {
      .entries = {initEntry(5,1)},
      .offset = 5, .snapshot = newSnapshot(4,1),
      .index = 5, .term = 1,  // stable to the first entry
      .woffset = 6, .wlen = 0,
    },      
    {
      .entries = {initEntry(5,1), initEntry(6,1)},
      .offset = 5, .snapshot = newSnapshot(4,1),
      .index = 5, .term = 1,  // stable to the first entry
      .woffset = 6, .wlen = 1,
    },  
    {
      .entries = {initEntry(6,2)},
      .offset = 6, .snapshot = newSnapshot(5,1),
      .index = 6, .term = 1,  // stable to the first entry and term mismatch
      .woffset = 6, .wlen = 1,
    }, 
    {
      .entries = {initEntry(5,1)},
      .offset = 5, .snapshot = newSnapshot(5,1),
      .index = 4, .term = 1,  // stable to snapshot
      .woffset = 5, .wlen = 1,
    },           
    {
      .entries = {initEntry(5,2)},
      .offset = 5, .snapshot = newSnapshot(4,2),
      .index = 4, .term = 1,  // stable to snapshot
      .woffset = 5, .wlen = 1,
    },       
  };

  size_t i;
  for (i = 0;i < SIZEOF_ARRAY(tests); ++i) {
    unstableLog unstable;
    unstable.entries_ = tests[i].entries;
    unstable.offset_  = tests[i].offset;
    unstable.snapshot_  = tests[i].snapshot;
    unstable.logger_  = NULL;

    unstable.stableTo(tests[i].index, tests[i].term);
    EXPECT_EQ(unstable.offset_, tests[i].woffset) << "i: " << i << ", woffset: " << tests[i].woffset;
    EXPECT_EQ((int)unstable.entries_.size(), tests[i].wlen);

    if (tests[i].snapshot != NULL) {
      delete tests[i].snapshot;
    }     
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
  } tests[] = {
    // append to the end
    {
      .entries = {initEntry(5,1),},
      .offset = 5, .snapshot = NULL,
      .toappend = {initEntry(6,1),initEntry(7,1),},
      .woffset = 5,
      .wentries = {initEntry(5,1),initEntry(6,1),initEntry(7,1)},
    },
    // replace the unstable entries
    {
      .entries = {initEntry(5,1),},
      .offset = 5, .snapshot = NULL,
      .toappend = {initEntry(5,2),initEntry(6,2),},
      .woffset = 5,
      .wentries = {initEntry(5,2),initEntry(6,2),},
    },   
    {
      .entries = {initEntry(5,1),},
      .offset = 5, .snapshot = NULL,
      .toappend = {initEntry(4,2),initEntry(5,2),initEntry(6,2),},
      .woffset = 4,
      .wentries = {initEntry(4,2),initEntry(5,2),initEntry(6,2),},
    }, 
    // truncate the existing entries and append   
    {
      .entries = {initEntry(5,1),initEntry(6,1),initEntry(7,1),},
      .offset = 5, .snapshot = NULL,
      .toappend = {initEntry(6,2),},
      .woffset = 5,
      .wentries = {initEntry(5,1),initEntry(6,2),},
    }, 
    {
      .entries = {initEntry(5,1),initEntry(6,1),initEntry(7,1),},
      .offset = 5, .snapshot = NULL,
      .toappend = {initEntry(7,2),initEntry(8,2),},
      .woffset = 5,
      .wentries = {initEntry(5,1),initEntry(6,1),initEntry(7,2),initEntry(8,2),},
    },            
  };

  size_t i;
  for (i = 0;i < SIZEOF_ARRAY(tests); ++i) {
    unstableLog unstable;
    unstable.entries_ = tests[i].entries;
    unstable.offset_  = tests[i].offset;
    unstable.snapshot_  = tests[i].snapshot;
    unstable.logger_  = &kDefaultLogger;

    unstable.truncateAndAppend(tests[i].toappend);
    EXPECT_EQ(unstable.offset_, tests[i].woffset) << "i: " << i << ", woffset: " << tests[i].woffset;
    EXPECT_TRUE(isDeepEqualEntries(unstable.entries_, tests[i].wentries)) << "i: " << i;

    if (tests[i].snapshot != NULL) {
      delete tests[i].snapshot;
    }     
  }
}
