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

array_t* array_create(size_t elem_size);

void array_destroy(array_t* array);

static inline size_t array_size(const array_t* array) {
  return array->size;
}

void* array_get(array_t *array, size_t index);

// erase elements in [from, to)
void array_erase(array_t *array, size_t from, size_t to);

static inline void array_clear(array_t *array) {
  array->size = 0;
}

void array_insert_array(array_t *array, size_t index, const array_t *a);

static inline size_t array_end(array_t *array) {
  return array->size;
}

void array_copy(array_t *array, array_t *from);

void array_push_batch(array_t *array, const void *data, int n);

static inline void array_push(array_t *array, const void *data) {
  array_push_batch(array, data, 1);
}

void array_assign(array_t *array, array_t* a, size_t from, size_t to);

void* array_pop(array_t *array);

#ifdef __cplusplus
}
#endif

#endif