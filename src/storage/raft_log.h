/*
 * Copyright (C) lichuang
 */

#ifndef __LIB_RAFT_LOG_H__
#define __LIB_RAFT_LOG_H__

#include "libraft.h"

struct raft_storage_t;
struct unstable_log_t;

typedef struct raft_log_t {
  // storage contains all stable entries since the last snapshot.
  struct raft_storage_t* storage;

  // unstable contains all unstable entries and snapshot.
  // they will be saved into storage.
  unstable_log_t* unstable;

  // committed is the highest log position that is known to be in
  // stable storage on a quorum of nodes.
  raft_index_t committed;

  // applied is the highest log position that the application has
  // been instructed to apply to its state machine.
  // Invariant: applied <= committed
  raft_index_t applied;    
} raft_log_t;

raft_log_t* raft_log_storage_create(struct raft_storage_t*);
void        raft_log_storage_destroy(raft_log_t*);

// raft_log_storage_maybe_append returns false if the entries cannot be appended. Otherwise,
// it returns last index of new entries.
bool         raft_log_storage_maybe_append(raft_log_t*, 
                                          raft_index_t index, raft_term_t log_term,
                                          raft_index_t committed, array_t*, raft_index_t* last);

raft_index_t raft_log_first_index(raft_log_t*);
raft_index_t raft_log_last_index(raft_log_t*);

// returns the term of the entry at index i, if there is any.
raft_error_e raft_log_term(raft_log_t*, raft_index_t i, raft_term_t* term);

bool raft_log_match_term(raft_log_t*, raft_index_t i, raft_term_t term);

// raft_log_find_conflict finds the index of the conflict.
// It returns the first pair of conflicting entries between the existing
// entries and the given entries, if there are any.
// If there is no conflicting entries, and the existing entries contains
// all the given entries, zero will be returned.
// If there is no conflicting entries, but the given entries contains new
// entries, the index of the first new entry will be returned.
// An entry is considered to be conflicting if it has the same index but
// a different term.
// The first entry MUST have an index equal to the argument 'from'.
// The index of the given entries MUST be continuously increasing.
raft_index_t raft_log_find_conflict(raft_log_t*, const array_t* entries);

// append entries to unstable storage and return last index
// fatal if the first index of entries < committed_
raft_index_t raft_log_append(raft_log_t*, const array_t* entries);

// change committed_ index to tocommit
void raft_log_commit_to(raft_log_t*, raft_index_t tocommit);

// change applied index to i
void raft_log_applied_to(raft_log_t*, raft_index_t i);

// unstable storage stable index to log
void raft_log_stable_to(raft_log_t*, raft_index_t i, raft_term_t term);

// unstable storage stable index to snapshot
void raft_log_stable_snap_to(raft_log_t*, raft_index_t i);

// check err code, if success return term,
// return 0 if error code is ErrCompacted
// others Fatal
raft_term_t raft_log_zero_term_on_err_compacted(raft_term_t t, raft_error_e err);

#endif /* __LIB_RAFT_LOG_H__ */