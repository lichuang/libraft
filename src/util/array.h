/*
 * Copyright (C) lichuang
 */

#ifndef __LIB_RAFT_ARRAY_H__
#define __LIB_RAFT_ARRAY_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct array_t {
  size_t size;
  size_t capacity;
  size_t elem_size;
  void* data;
} array_t;

void array_init(array_t*, size_t elem_size);

void array_destroy(array_t* array);

inline size_t array_size(array_t* array) {
  return array->size;
}

void* array_get(array_t *array, size_t index);

void array_erase(array_t *array, size_t from, size_t to);

inline void array_clear(array_t *array) {
  array->size = 0;
}

void array_insert_array(array_t *array, size_t index, const array_t *a);

inline size_t array_end(array_t *array) {
  return array->size;
}

void array_copy(array_t *array, array_t *from);

void array_push_batch(array_t *array, const void *data, int n);

void array_push(array_t *array, const void *data);

void* array_pop(array_t *array);

#ifdef __cplusplus
}
#endif

#endif