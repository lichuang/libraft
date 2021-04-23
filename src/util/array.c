/*
 * Copyright (C) lichuang
 */

#include <stdlib.h>
#include <string.h>
#include "util/assert.h"
#include "util/array.h"

const static size_t min_size = 8;

#define ARRAY_GET_ELEM(array, index) ((void*)((char*)((array)->data) + (index) * (array)->elem_size))

array_t* 
array_create(size_t elem_size) {
  array_t *array = (array_t*)malloc(sizeof(array_t));
  *array = (array_t) {
    .size = 0,
    .capacity = min_size,
    .elem_size = elem_size,
    .data = malloc(elem_size * min_size),
  };

  return array;
}

void 
array_destroy(array_t* array) {
  free(array->data);
  free(array);
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
  ASSERT(index < array->size);
  
  void* d = ARRAY_GET_ELEM(array, index);
  
  //return *(void**)d;
  return d;
}

void 
array_erase(array_t *array, size_t from, size_t to) {
  ASSERT(to < array->size && from < to);

  char *start = (char*)array->data + from * array->elem_size;
  char *end   = (char*)array->data + to  * array->elem_size;
  size_t remain = array->size - to;
  memmove(start, end, remain * array->elem_size);
  array->size -= to - from;
}

void 
array_insert_array(array_t *array, size_t index, const array_t *a) {
  ASSERT(index < array->size);
  ASSERT(array->elem_size == a->elem_size);

  size_t a_size = array_size(a);
  ensure_array_size(array, a_size);
  
  char *start = (char*)array->data + (array->size+a_size) * array->elem_size;
  char *end   = (char*)array->data + index * array->elem_size;
  
  // first:move hole space
  size_t remain = array->size - index;
  memmove(start, end, remain * array->elem_size);

  // second:add new array
  memmove(end, a->data, a_size * array->elem_size);

  array->size += a_size;
}

void 
array_copy(array_t *array, array_t *from) {
  ASSERT(array->elem_size == from->elem_size);
  size_t a_size = array_size(from);
  
  ensure_array_size(array, a_size);

  char *start = (char*)array->data;
  char *start_from   = (char*)from->data;
  memmove(start, start_from, a_size * array->elem_size);

  array->size = a_size;
}
