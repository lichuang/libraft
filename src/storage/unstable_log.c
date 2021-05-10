/*
 * Copyright (C) lichuang
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "util/log.h"
#include "unstable_log.h"

static entry_t k_empty_entry = (entry_t) { 
  .index = 0,
  .term  = 0, 
};

unstable_log_t* 
unstable_log_create() { 
  unstable_log_t* unstable = (unstable_log_t*)malloc(sizeof(unstable_log_t));
  
  *unstable = (unstable_log_t) {
    .snapshot = NULL,
    .offset = 0,
    .entries = array_create(sizeof(entry_t)),
  };

  return unstable;
}

void 
unstable_log_destroy(unstable_log_t* unstable) {
  array_destroy(unstable->entries);
  if (unstable->snapshot) {
    free(unstable->snapshot);
  }
  free(unstable);
}

bool 
unstable_log_maybe_first_index(unstable_log_t* unstable, raft_index_t* first) {
  if (unstable->snapshot != NULL) {
    *first = unstable->snapshot->meta.index + 1;
    return true;
  }

  *first = 0;
  return false;
}

bool 
unstable_log_maybe_last_index(unstable_log_t* unstable, raft_index_t* last) {  
  // first check entries
  if (array_size(unstable->entries) > 0) {
    *last = unstable->offset + array_size(unstable->entries) - 1;
    return true;
  }

  // then check snapshot
  if (unstable->snapshot != NULL) {
    *last = unstable->snapshot->meta.index;
    return true;
  }

  *last = 0;
  return false;
}

bool 
unstable_log_maybe_term(unstable_log_t* unstable, raft_index_t i, raft_term_t* term) {
  if (i < unstable->offset) {
    if (unstable->snapshot == NULL) {
      *term = 0;
      return false;
    }
    if (unstable->snapshot->meta.index == i) {
      *term = unstable->snapshot->meta.term;
      return true;
    }
    *term = 0;
    return false;
  }

  raft_index_t last;
  bool ok = unstable_log_maybe_last_index(unstable, &last);
  if (!ok) {
    *term = 0;
    return false;
  }
  if (i > last) {
    *term = 0;
    return false;
  }

  entry_t* entry = array_get(unstable->entries, i - unstable->offset);
  *term = entry->term;
  return true;
}

void 
unstable_log_stable_to(unstable_log_t* unstable, raft_index_t i, raft_term_t t) {  
  raft_term_t gt;
  bool ok = unstable_log_maybe_term(unstable, i, &gt);
  if (!ok) {
    return;
  }

  // if i < offset, term is matched with the snapshot
  // only update the unstable entries if term is matched with
  // an unstable entry.
  raft_index_t offset = unstable->offset;
  if (gt == t && i >= offset) {
    array_erase(unstable->entries, 0, i + 1 - offset);
    unstable->offset = i + 1;
    Debugf("stable to %llu, entries size:%d, offset:%llu", i, array_size(unstable->entries), unstable->offset);
  }
}

void 
unstable_log_stable_snap_to(unstable_log_t* unstable, raft_index_t i) {
  if (unstable->snapshot == NULL) {
    return;
  }
  if (unstable->snapshot->meta.index != i) {
    return;
  }

  free(unstable->snapshot);
  unstable->snapshot = NULL;
}

void 
unstable_log_restore(unstable_log_t* unstable, const snapshot_t* snapshot) {
  unstable->offset = snapshot->meta.index + 1;
  array_clear(unstable->entries);
  if (unstable->snapshot == NULL) {
    unstable->snapshot = (snapshot_t*)malloc(sizeof(snapshot_t));
  }
  memcpy(unstable->snapshot, snapshot, sizeof(snapshot_t));
}

void
unstable_log_truncate_and_append(unstable_log_t* unstable, array_t* entries) {
  if (array_size(entries) == 0) {
    return;
  }
  entry_t* entry = array_get(entries, 0);
  raft_index_t after = entry->index;
  raft_index_t offset = unstable->offset;
  if (after == offset + array_size(unstable->entries)) {
    // after is the next index in the u.entries
    // directly append
    array_append_array(unstable->entries, entries);
    Infof("ENTRY size: %lld", array_size(unstable->entries));
    return;    
  }

  if (after <= offset) {
    // The log is being truncated to before our current offset
    // portion, so set the offset and replace the entries
    unstable->offset = after;
    array_copy(unstable->entries, entries);
    Infof("replace the unstable entries from index %llu", after);
    return;
  }

  // truncate to after and copy to u.entries then append
  Infof("truncate the unstable entries before index %llu", after);
  array_t *slice = array_create(sizeof(entry_t));  
  unstable_log_slice(unstable, offset, after, slice);
  
#if 0
  array_assign(unstable->entries, &k_empty_entry);
  array_append_array(unstable->entries, slice);
#else  
  array_copy(unstable->entries, slice);
#endif

  array_destroy(slice);
  array_append_array(unstable->entries, entries);
}

static bool
must_check_out_of_bounds(unstable_log_t* unstable,raft_index_t lo, raft_index_t hi) {
  if (lo > hi) {
    Fatalf("invalid unstable.slice %llu > %llu", lo, hi);
    return false;
  }

  raft_index_t offset = unstable->offset;
  raft_index_t upper = offset + offset + array_size(unstable->entries);
  if (lo < offset || upper < hi) {
    Fatalf("unstable.slice[%llu,%llu) out of bound [%llu,%llu]", lo, hi, offset, upper);
    return false;
  }

  return true;
}

void 
unstable_log_slice(unstable_log_t* unstable, raft_index_t lo, raft_index_t hi, array_t* entries) {
  assert(entries->elem_size == sizeof(entry_t));

  if (!must_check_out_of_bounds(unstable, lo, hi)) {
    return;
  }

  array_assign_array(entries, unstable->entries, lo - unstable->offset, hi - unstable->offset);
}