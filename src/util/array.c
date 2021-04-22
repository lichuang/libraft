/*
 * Copyright (C) lichuang
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "util/array.h"

const static size_t min_size = 8;

#define ARRAY_GET_ELEM(array, index) ((void*)((char*)((array)->data) + (index) * (array)->elem_size))

void 
array_init(array_t* array, size_t elem_size) {
  *array = (array_t) {
    .size = 0,
    .capacity = min_size,
    .elem_size = elem_size,
    .data = malloc(elem_size * min_size),
  };
}

void 
array_destroy(array_t* array) {
  free(array->data);
}

static inline void
ensure_array_size(array_t *array, int n) {
  if (array->capacity - array->size >= n) {
    return;
  }

  size_t tsize = (array->capacity << 1u);
  while (array->size + n > tsize) {
    tsize = (tsize << 1u);
  }

  array->capacity = tsize;
  array->data = realloc(array->data, array->elem_size * array->capacity);
}

void 
array_push_batch(array_t *array, const void *data, int n) {
  ensure_array_size(array, n);

  void* dst = ARRAY_GET_ELEM(array, array->size);
  memcpy(dst, data, array->elem_size * n);

  array->size += n;
}

void 
array_push(array_t *array, const void *data) {
  array_push_batch(array, data, 1);
}

void* 
array_pop(array_t *array) {
  if (array->size == 0) {
    return NULL;
  }
  array->size -= 1;
  return ARRAY_GET_ELEM(array, array->size);  
}

void* 
array_get(array_t *array, size_t index) {
  assert(index < array->size);
  
  void* d = ARRAY_GET_ELEM(array, index);
  
  return *(void**)d;
}

void 
array_erase(array_t *array, size_t from, size_t to) {
  assert(to < array->size && from < to);


}

void 
array_insert_array(array_t *array, size_t index, const array_t *a) {

}

void 
array_copy(array_t *array, array_t *from) {
  assert(array->elem_size == from->elem_size);

}
