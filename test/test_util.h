/*
 * Copyright (C) lichuang
 */

#ifndef __LIBRAFT_RAFT_TEST_UTIL_H__
#define __LIBRAFT_RAFT_TEST_UTIL_H__

#include <stdlib.h>
#include "libraft.h"

static inline entry_t*
init_test_entry(uint64_t index, uint64_t term) { 
  entry_t* entry = (entry_t*)malloc(sizeof(entry_t));
  *entry = (entry_t) {
    .index = index,
    .term = term,
    //.data = data,
  };

  return entry;
}

static void
free_test_entry(void* entry) {
  free(entry);
}

static inline snapshot_t*
create_test_snapshot(raft_index_t index, raft_term_t term) {
  snapshot_t* sn = (snapshot_t*)malloc(sizeof(snapshot_t));
  *sn = (snapshot_t) {
    .meta = (snapshot_meta_t) {
      .index = index,
      .term = term,
    }
  };  

  return sn;
}

static inline void
destroy_test_snapshot(snapshot_t* sn) {
  if (sn) {
    free(sn);
  }
}

bool is_deep_equal_snapshot(snapshot_t* s1, snapshot_t* s2) {
  if (s1 == NULL || s2 == NULL) {
    return false;
  }

  if (s1->meta.index != s2->meta.index || s1->meta.term != s2->meta.term) {
    return false;
  }

  return true;
}

#define SIZEOF_ARRAY(array) sizeof(array) / sizeof(array[0])

#endif