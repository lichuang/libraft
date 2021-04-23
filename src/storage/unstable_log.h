/*
 * Copyright (C) lichuang
 */

#ifndef __LIBRAFT_UNSTABLE_LOG_H__
#define __LIBRAFT_UNSTABLE_LOG_H__

#include <stdio.h>
#include "libraft.h"
#include "util/array.h"

// unstable.entries[i] has raft log position i+unstable.offset.
// Note that unstable.offset may be less than the highest log
// position in storage; this means that the next write to storage
// might need to truncate the log before persisting unstable.entries.
typedef struct unstable_log_t {
  // the incoming unstable snapshot, if any.
  snapshot_t* snapshot;

  // all entries that have not yet been written to storage.
  array_t* entries;

  uint64_t offset;


} unstable_log_t;

unstable_log_t* unstable_log_create();
void unstable_log_destroy(unstable_log_t*);

// unstable_log_maybe_first_index returns the index of the first 
// possible entry in entries if it has a snapshot.
bool unstable_log_maybe_first_index(unstable_log_t*, raft_index_t* first);

// unstable_log_maybe_last_index returns the last index if it has at least one
// unstable entry or snapshot.
bool unstable_log_maybe_last_index(unstable_log_t*, raft_index_t* last);

// unstable_log_maybe_term returns the term of the entry at index i, 
// if there is any.
bool unstable_log_maybe_term(unstable_log_t*, raft_index_t i, raft_term_t* term);

void unstable_log_stable_to(unstable_log_t*, raft_index_t i, raft_term_t t);

void unstable_log_stable_snap_to(unstable_log_t*, raft_index_t i);

void unstable_log_restore(unstable_log_t*, const snapshot_t*);

void unstable_log_truncate_and_append(unstable_log_t*, array_t* entries);

void unstable_log_slice(unstable_log_t*, raft_index_t lo, raft_index_t hi, array_t* entries);

#endif