/*
 * Copyright (C) lichuang
 */

#include "ctest.h"
#include "libraft.h"
#include "test_util.h"
#include "storage/unstable_log.h"

CTEST(unstable_log_test, TestUnstableMaybeFirstIndex) {
  struct tmp {
    array_t* entries;
    raft_index_t offset;
    snapshot_t* snapshot;
    bool wok;
    raft_index_t windex;
  } tests[] = {
    // no snapshot
    {
      .entries = array_createf(sizeof(entry_t), true,  init_test_entry(5,1),NULL),
      .offset = 5, .snapshot = NULL,
      .wok = false, .windex = 0,
    },
    {
      .entries = array_create(sizeof(entry_t)),
      .offset = 0, .snapshot = NULL,
      .wok = false, .windex = 0,
    },    
    // has snapshot
    {
      .entries = array_createf(sizeof(entry_t), true,  init_test_entry(5,1),NULL),
      .offset = 5, .snapshot = create_test_snapshot(4,1),
      .wok = true, .windex = 5,
    }, 
    {
      .entries = array_create(sizeof(entry_t)),
      .offset = 5, .snapshot = create_test_snapshot(4,1),
      .wok = true, .windex = 5,
    },           
  };

  size_t i;
  for (i = 0;i < SIZEOF_ARRAY(tests); ++i) {  
    unstable_log_t* unstable = unstable_log_create();

    array_copy(unstable->entries, tests[i].entries);
    unstable->offset = tests[i].offset;
    unstable->snapshot = tests[i].snapshot;

    raft_index_t index;
    bool ok = unstable_log_maybe_first_index(unstable, &index);
    ASSERT_EQUAL(ok, tests[i].wok);
    ASSERT_EQUAL(index, tests[i].windex);

    array_destroy(tests[i].entries);    

    unstable_log_destroy(unstable);
  }
}

CTEST(unstable_log_test, TestMaybeLastIndex) {
  struct tmp {
    array_t* entries;
    raft_index_t offset;
    snapshot_t* snapshot;
    bool wok;
    raft_index_t windex;
  } tests[] = {
    // last in entries
    {
      .entries = array_createf(sizeof(entry_t), true,  init_test_entry(5,1),NULL),
      .offset = 5, .snapshot = NULL,
      .wok = true, .windex = 5,
    },    
    {
      .entries = array_createf(sizeof(entry_t), true,  init_test_entry(5,1),NULL),
      .offset = 5, .snapshot = create_test_snapshot(4,1),
      .wok = true, .windex = 5,
    }, 
    // last in snapshot
    {
      .entries = array_create(sizeof(entry_t)),
      .offset = 5, .snapshot = create_test_snapshot(4,1),
      .wok = true, .windex = 4,
    },       
    // empty unstable
    {
      .entries = array_create(sizeof(entry_t)),
      .offset = 0, .snapshot = NULL,
      .wok = false, .windex = 0,
    },      
  };

  size_t i;
  for (i = 0;i < SIZEOF_ARRAY(tests); ++i) {  
    unstable_log_t* unstable = unstable_log_create();

    array_copy(unstable->entries, tests[i].entries);
    unstable->offset = tests[i].offset;
    unstable->snapshot = tests[i].snapshot;

    raft_index_t index;
    bool ok = unstable_log_maybe_last_index(unstable, &index);
    ASSERT_EQUAL(ok, tests[i].wok);
    ASSERT_EQUAL(index, tests[i].windex);

    array_destroy(tests[i].entries);

    unstable_log_destroy(unstable);
  }
}

CTEST(unstable_log_test, TestUnstableMaybeTerm) {
  struct tmp {
    array_t* entries;
    raft_index_t offset;
    snapshot_t* snapshot;
    raft_index_t index;
    bool wok;
    raft_term_t wterm;
  } tests[] = {
    // term from entries
    {
      .entries = array_createf(sizeof(entry_t), true,  init_test_entry(5,1),NULL),
      .offset = 5, .snapshot = NULL,
      .index = 5, .wok = true, .wterm = 1,
    },    
    {
      .entries = array_createf(sizeof(entry_t), true,  init_test_entry(5,1),NULL),
      .offset = 5, .snapshot = NULL,
      .index = 6, .wok = false, .wterm = 0,
    }, 
    {
      .entries = array_createf(sizeof(entry_t), true,  init_test_entry(5,1),NULL),
      .offset = 5, .snapshot = NULL,
      .index = 4, .wok = false, .wterm = 0,
    },  
    {
      .entries = array_createf(sizeof(entry_t), true,  init_test_entry(5,1),NULL),
      .offset = 5, .snapshot = create_test_snapshot(4,1),
      .index = 5, .wok = true, .wterm = 1,
    },        
    {
      .entries = array_createf(sizeof(entry_t), true,  init_test_entry(5,1),NULL),
      .offset = 5, .snapshot = create_test_snapshot(4,1),
      .index = 6, .wok = false, .wterm = 0,
    }, 
    // term from snapshot
    {
      .entries = array_createf(sizeof(entry_t), true,  init_test_entry(5,1),NULL),
      .offset = 5, .snapshot = create_test_snapshot(4,1),
      .index = 4, .wok = true, .wterm = 1,
    },
    {
      .entries = array_createf(sizeof(entry_t), true,  init_test_entry(5,1),NULL),
      .offset = 5, .snapshot = create_test_snapshot(4,1),
      .index = 3, .wok = false, .wterm = 0,
    }, 
    {
      .entries = array_create(sizeof(entry_t)),
      .offset = 5, .snapshot = create_test_snapshot(4,1),
      .index = 5, .wok = false, .wterm = 0,
    }, 
    {
      .entries = array_create(sizeof(entry_t)),
      .offset = 5, .snapshot = create_test_snapshot(4,1),
      .index = 4, .wok = true, .wterm = 1,
    },  
    {
      .entries = array_create(sizeof(entry_t)),
      .offset = 0, .snapshot = NULL,
      .index = 5, .wok = false, .wterm = 0,
    },                               
  };

  size_t i;
  for (i = 0;i < SIZEOF_ARRAY(tests); ++i) {  
    unstable_log_t* unstable = unstable_log_create();

    array_copy(unstable->entries, tests[i].entries);

    unstable->offset = tests[i].offset;
    unstable->snapshot = tests[i].snapshot;
    
    raft_term_t term;
    bool ok = unstable_log_maybe_term(unstable, tests[i].index, &term);
    ASSERT_EQUAL(ok, tests[i].wok);
    ASSERT_EQUAL(term, tests[i].wterm);

    array_destroy(tests[i].entries);

    unstable_log_destroy(unstable);
  }
}

CTEST(unstable_log_test, TestUnstableRestore) {
  unstable_log_t* unstable = unstable_log_create();
  array_push(unstable->entries, init_test_entry(5,1));
  unstable->offset = 5;
  unstable->snapshot = create_test_snapshot(4, 1);

  snapshot_t s;
  s.meta.index = 6;
  s.meta.term  = 2;

  unstable_log_restore(unstable, &s);

  ASSERT_EQUAL(unstable->offset, s.meta.index + 1);
  ASSERT_EQUAL(array_size(unstable->entries), 0);
  ASSERT_EQUAL(is_deep_equal_snapshot(unstable->snapshot, &s), true);

  unstable_log_destroy(unstable);
}

CTEST(unstable_log_test, TestUnstableStableTo) {
  struct tmp {
    array_t* entries;
    raft_index_t offset;
    snapshot_t* snapshot;
    raft_index_t index;
    raft_term_t term;
    raft_index_t woffset;
    int wlen;
  } tests[] = {
    {
      .entries = array_create(sizeof(entry_t)),
      .offset = 0, .snapshot = NULL,
      .index = 5, .term = 1,
      .woffset = 0, .wlen = 0,
    },
    {
      .entries = array_createf(sizeof(entry_t), true,  init_test_entry(5,1),NULL),
      .offset = 5, .snapshot = NULL,
      .index = 5, .term = 1,  // stable to the first entry
      .woffset = 6, .wlen = 0,
    },    
    {
      .entries = array_createf(sizeof(entry_t), true,  init_test_entry(5,1), init_test_entry(6,1), NULL),
      .offset = 5, .snapshot = NULL,
      .index = 5, .term = 1,  // stable to the first entry
      .woffset = 6, .wlen = 1,
    },
    {
      .entries = array_createf(sizeof(entry_t), true,  init_test_entry(6,2), NULL),
      .offset = 6, .snapshot = NULL,
      .index = 6, .term = 1,  // stable to the first entry and term mismatch
      .woffset = 6, .wlen = 1,
    },
    {
      .entries = array_createf(sizeof(entry_t), true,  init_test_entry(5,1), NULL),
      .offset = 5, .snapshot = NULL,
      .index = 4, .term = 1,  // stable to old entry
      .woffset = 5, .wlen = 1,
    },
    {
      .entries = array_createf(sizeof(entry_t), true,  init_test_entry(5,1), NULL),
      .offset = 5, .snapshot = NULL,
      .index = 4, .term = 2,  // stable to old entry
      .woffset = 5, .wlen = 1,
    },
    // with snapshot
    {
      .entries = array_createf(sizeof(entry_t), true,  init_test_entry(5,1), NULL),
      .offset = 5, .snapshot = create_test_snapshot(4,1),
      .index = 5, .term = 1,  // stable to the first entry
      .woffset = 6, .wlen = 0,
    },
    {
      .entries = array_createf(sizeof(entry_t), true,  init_test_entry(5,1), init_test_entry(6,1),NULL),
      .offset = 5, .snapshot = create_test_snapshot(4,1),
      .index = 5, .term = 1,  // stable to the first entry
      .woffset = 6, .wlen = 1,
    },
    {
      .entries = array_createf(sizeof(entry_t), true,  init_test_entry(6,2), NULL),
      .offset = 6, .snapshot = create_test_snapshot(5,1),
      .index = 6, .term = 1,  // stable to the first entry and term mismatch
      .woffset = 6, .wlen = 1,
    },
    {
      .entries = array_createf(sizeof(entry_t), true,  init_test_entry(5,1), NULL),
      .offset = 5, .snapshot = create_test_snapshot(4,1),
      .index = 4, .term = 1,  // stable to snapshot
      .woffset = 5, .wlen = 1,
    },
    {
      .entries = array_createf(sizeof(entry_t), true,  init_test_entry(5,2), NULL),
      .offset = 5, .snapshot = create_test_snapshot(4,2),
      .index = 4, .term = 1,  // stable to old entry
      .woffset = 5, .wlen = 1,
    },                                        
  };

  size_t i;
  for (i = 0;i < SIZEOF_ARRAY(tests); ++i) {  
    unstable_log_t* unstable = unstable_log_create();
    struct tmp *test = &(tests[i]);

    array_copy(unstable->entries, test->entries);

    unstable->offset = test->offset;
    unstable->snapshot = test->snapshot;
    
    unstable_log_stable_to(unstable, test->index, test->term);

    ASSERT_EQUAL(unstable->offset, test->woffset);
    ASSERT_EQUAL(array_size(unstable->entries), test->wlen);

    array_destroy(test->entries);

    unstable_log_destroy(unstable);
  }  
}

