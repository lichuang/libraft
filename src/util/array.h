/*
 * Copyright (C) lichuang
 */

#ifndef __LIB_RAFT_ARRAY_H__
#define __LIB_RAFT_ARRAY_H__

#include <stddef.h>
/* for varags */
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct array_t array_t;

struct array_t {
  size_t size;
  size_t capacity;
  size_t elem_size;
  void* data;

  // function methods
  void (*free) (void*);
  void* (*dup) (void*);
};

array_t* array_create(size_t elem_size);

array_t* array_createf(size_t elem_size, void* args,...);

static inline void array_set_free(array_t *array, void (*free) (void*)) {
  array->free = free;
}

static inline void array_set_dup(array_t *array, void* (*dup) (void*)) {
  array->dup = dup;
}

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

array_t* array_push_batch(array_t *array, void *data, int n);

static inline array_t* array_push(array_t *array, void *data) {
  return array_push_batch(array, data, 1);
}

void array_assign(array_t *array, array_t* a, size_t from, size_t to);

void* array_pop(array_t *array);

#ifdef __cplusplus
}
#endif

#endif