/*
 * Copyright (C) lichuang
 */

#ifndef __LIB_RAFT_MEMORY_STORAGE_H__
#define __LIB_RAFT_MEMORY_STORAGE_H__

struct array_t;

raft_storage_t* create_memory_storage(array_t*);

#endif  // __LIB_RAFT_MEMORY_STORAGE_H__