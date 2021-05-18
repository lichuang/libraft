/*
 * Copyright (C) lichuang
 */

#include <stdlib.h>
#include "util/log.h"
#include "raft_log.h"
#include "unstable_log.h"

raft_log_t* 
raft_log_storage_create(struct raft_storage_t* storage) {
  raft_log_t* log = (raft_log_t*) malloc(sizeof(raft_log_t));

  *log = (raft_log_t) {
    .storage = storage,
    .unstable = unstable_log_create(),
    .committed = 0,
    .applied = 0,
  };

  return log;
}

void                
raft_log_storage_destroy(struct raft_log_t* log) {
  if (log->storage) {
    free(log->storage);
  }
  unstable_log_destroy(log->unstable);
  free(log);
}

bool                
raft_log_storage_maybe_append(raft_log_t* log, 
                              raft_index_t index, raft_term_t log_term,
                              raft_index_t committed, array_t* entries, raft_index_t* last) {
  *last = 0;

  // check if log index and term match
  if (!raft_log_match_term(log, index, log_term)) {
    return false;
  }

  raft_index_t last_new_i, ci, offset;

  last_new_i = index + (raft_index_t)array_size(entries);
  // check if there is conflict entries
  ci = raft_log_find_conflict(log, entries);
  if (ci != 0 && ci <= log->committed) {
    Fatalf("entry %llu conflict with committed entry [committed(%llu)]", ci, log->committed);
  }
  
  if (ci != 0) {
    offet = index + 1;
  }
  return true;
}

bool
raft_log_match_term(raft_log_t* log, raft_index_t i, raft_term_t term) {
  raft_error_e err;
  raft_term_t t;

  err = raft_log_term(log, i, &t);
  if (!SUCCESS(err)) {
    return false;
  }

  return t == term;
}

void 
raft_log_commit_to(raft_log_t* log, raft_index_t tocommit) {
  // never decrease commit
  if (log->committed >= tocommit) {
    return;
  }

  // to commit index cannot bigger than last index
  raft_index_t lastIndex = raft_log_last_index(log);
  if (lastIndex < tocommit) {
    Fatalf("tocommit(%llu) is out of range [lastIndex(%llu)]. Was the raft log corrupted, truncated, or lost?",
      tocommit, lastIndex);
  }
  log->committed = tocommit;
  Debugf("commit to %llu", tocommit);
}

void 
raft_log_applied_to(raft_log_t* log, raft_index_t i) {
  if (i == 0) {
    return;
  }

  // applied index cannot bigger than committed index,
  // also cannot smaller than already applied index.
  if (log->committed < i || i < log->applied) {
    Fatalf("applied(%llu) is out of range [prevApplied(%llu), committed(%llu)]", i, log->applied, log->committed);
  }

  log->applied = i;
}

void 
raft_log_stable_to(raft_log_t* log, raft_index_t i, raft_term_t term) {
  unstable_log_stable_to(log->unstable, i, term);
}

void 
raft_log_stable_snap_to(raft_log_t* log, raft_index_t i) {
  unstable_log_stable_snap_to(log->unstable, i);
}

raft_index_t 
raft_log_append(raft_log_t* log, const array_t* entries) {
  if (array_empty(entries)) {
    return raft_log_last_index(log);
  }

  const entry_t* entry = array_get(entries, 0);
  raft_index_t after = entry->index - 1;
  if (after < log->committed) {
    Fatalf("after(%llu) is out of range [committed(%llu)]", after, log->committed);
  }

  unstable_log_truncate_and_append(log->unstable, entries);
  return raft_log_last_index(log);
}

// returns the term of the entry at index i, if there is any.
static raft_error_e 
raft_log_term(raft_log_t* log, raft_index_t i, raft_term_t* term) {
  raft_index_t dummy_index;
  raft_error_e err = OK;

  *term = 0;
  // the valid term range is [index of dummy entry, last index]
  dummy_index = raft_log_first_index(log) - 1;
  if (i < dummy_index || i > raft_log_last_index(log)) {
    return OK;
  }

  // first check in unstable storage
  bool ok = unstable_log_maybe_term(log->unstable, i, term);
  if (ok) {
    return OK;
  }

  // then check in stable storage
  err = log->storage->term(log->storage, i, term);
  if (SUCCESS(err)) {
    return err;
  }

  if (err == ErrCompacted || err == ErrUnavailable) {
    return err;
  }

  Fatalf("term err:%s", k_raft_err_string[err]);

  return err;
}

raft_index_t 
raft_log_first_index(raft_log_t* log) {
  raft_index_t i;
  raft_error_e err;

  bool ok = unstable_log_maybe_first_index(log->unstable, &i);
  if (ok) {
    return i;
  }

  err = log->storage->first_index(log->storage, &i);
  if (!SUCCESS(err)) {
    Fatalf("firstIndex error:%s", k_raft_err_string[err]);    
  }

  return i;
}

raft_index_t 
raft_log_last_index(raft_log_t* log) {
  raft_index_t i;
  raft_error_e err;

  bool ok = unstable_log_maybe_last_index(log->unstable, &i);
  if (ok) {
    return i;
  }

  err = log->storage->last_index(log->storage, &i);
  if (!SUCCESS(err)) {
    Fatalf("lastIndex error:%s", k_raft_err_string[err]);    
  }

  return i;
}

raft_index_t 
raft_log_find_conflict(raft_log_t* log, const array_t* entries) {
  size_t size = array_size(entries);
  size_t i;
  for (i = 0; i < size; i++) {
    const entry_t* entry = array_get(entries, i);
    if (!raft_log_match_term(log, entry->index, entry->term)) {
      raft_index_t index = entry->index;
      raft_term_t term   = entry->term;
      if (index <= raft_log_last_index(log)) {
        raft_term_t dummy;
        raft_error_e err = raft_log_term(log, index, &dummy);
        Infof("found conflict at index %llu [existing term: %llu, conflicting term: %llu]",
          index, raft_log_zero_term_on_err_compacted(dummy, err), term);
      }

      return index;
    }
  }

  return 0;
}

raft_term_t 
raft_log_zero_term_on_err_compacted(raft_term_t t, raft_error_e err) { 
  if (SUCCESS(err)) {
    return t;
  }

  if (err == ErrCompacted) {
    return 0;
  }

  Fatalf("unexpected error: %s", k_raft_err_string[err]);
  return 0;
}