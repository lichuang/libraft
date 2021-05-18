/*
 * Copyright (C) lichuang
 */

#include <stdlib.h>
#include "libraft.h"
#include "memory_storage.h"

static raft_error_e __init_state(raft_storage_t* s, hard_state_t *hs, conf_state_t *cs);
static raft_error_e __first_index(raft_storage_t* s, raft_index_t *index);
static raft_error_e __last_index(raft_storage_t* s, raft_index_t *index);
static raft_error_e __term(raft_storage_t* s, raft_index_t i, raft_term_t *term);
static raft_error_e __entries(raft_storage_t* s, raft_index_t lo, raft_index_t hi, uint64_t maxSize, array_t *entries);
static raft_error_e __get_snapshot(raft_storage_t* s, snapshot_t **snapshot);

static raft_storage_t memory_storage;

typedef struct memory_storage_private_t {
  array_t *entries;

  hard_state_t hard_state;

  snapshot_t* snapshot;

} memory_storage_private_t ;

raft_storage_t*
create_memory_storage(array_t* entries) {
  raft_storage_t* storage = (raft_storage_t*)malloc(sizeof(raft_storage_t));

  *storage = (raft_storage_t) {
    .init_state   = __init_state,
    .first_index  = __first_index,
    .last_index   = __last_index,
    .term         = __term,
    .entries      = __entries,
    .get_snapshot = __get_snapshot,
  };
  memory_storage_private_t* pri = (memory_storage_private_t*)storage->data;
  pri->snapshot = NULL;
  pri->entries = array_create(sizeof(entry_t));
  
  if (entries == NULL) {
    // When starting from scratch populate the list with a dummy entry at term zero.
  } else {

  }

  return storage;
}

static raft_error_e 
__init_state(raft_storage_t* s, hard_state_t *hs, conf_state_t *cs) {
  memory_storage_private_t* pri = (memory_storage_private_t*)s->data;
  *hs = pri->hard_state;
  if (pri->snapshot) {
    *cs = pri->snapshot->meta.conf_state;
  }
  return OK;
}

static raft_error_e 
__first_index(raft_storage_t* s, raft_index_t *index) {
  memory_storage_private_t* pri = (memory_storage_private_t*)s->data;
  entry_t* entry = array_get(pri->entries, 0);

  *index = entry->index + 1;
  return OK;
}

static raft_error_e 
__last_index(raft_storage_t* s, raft_index_t *index) {
  memory_storage_private_t* pri = (memory_storage_private_t*)s->data;

  entry_t* entry = array_get(pri->entries, 0);
  *index = entry->index + array_size(pri->entries) - 1;

  return OK;
}

static raft_error_e 
__term(raft_storage_t* s, raft_index_t i, raft_term_t *term) {
  memory_storage_private_t* pri = (memory_storage_private_t*)s->data;

  *term = 0;
  entry_t* entry = array_get(pri->entries, 0);
  raft_index_t offset = entry->index;
  if (i < offset) {
    return ErrCompacted;
  }
  if (i - offset >= array_size(pri->entries)) {
    return ErrUnavailable;
  }
  entry = array_get(pri->entries, i-offset);
  *term = entry->term;
  return OK;
}

static raft_error_e 
__entries(raft_storage_t* s, raft_index_t lo, raft_index_t hi, uint64_t maxSize, array_t *entries) {
  memory_storage_private_t* pri = (memory_storage_private_t*)s->data;

  // first check validity of index
  entry_t* entry = array_get(pri->entries, 0);
  raft_index_t offset = entry->index;
  if (lo <= offset) {
    return ErrCompacted;
  }
  if (hi > entry->index + array_size(pri->entries) - 1) {
    return ErrUnavailable;
  }

  // only contains dummy entries.
  if (array_size(pri->entries) == 1) {
    return ErrUnavailable;
  }
  raft_index_t i;
  for (i = lo - offset; i < hi - offset; i++) {
    array_push(entries, array_get(entries, i));
  }

  //limitSize(maxSize, entries);
  return OK;
}

static raft_error_e
__get_snapshot(raft_storage_t* s, snapshot_t **snapshot) {
  memory_storage_private_t* pri = (memory_storage_private_t*)s->data;
  *snapshot = pri->snapshot;

  return OK;
}