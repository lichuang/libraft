/*
 * Copyright (C) lichuang
 */

#include <stdlib.h>
#include <string.h>
#include "util/array.h"
#include "util/assert.h"

const static size_t k_min_size = 8;

#define ARRAY_GET_ELEM(array, index) ((void*)((char*)((array)->data) + (index) * (array)->elem_size))

array_t* 
array_create(size_t elem_size) {
  array_t *array = (array_t*)malloc(sizeof(array_t));
  *array = (array_t) {
    .size = 0,
    .capacity = k_min_size,
    .elem_size = elem_size,
    .data = malloc(elem_size * k_min_size),
    .free = NULL,
    .cmp  = NULL,
  };

  return array;
}

array_t* 
array_createf(bool delete_after_init, size_t elem_size, ...) {
  array_t *array = array_create(elem_size);
  void *arg = NULL;

  va_list ap;
  va_start(ap, elem_size);
  arg = va_arg(ap,void*);
  while (arg != NULL) {
    array_push(array, arg);
    arg = va_arg(ap,void*);
    if (delete_after_init) {
      free(arg);
    }
  }
  va_end(ap);

  return array;
}

void 
array_destroy(array_t* array) {
  if (array->free) {
    size_t i;
    for (i = 0; i < array->size; i++) {
      array->free(ARRAY_GET_ELEM(array, i));
    }
  } else {
    free(array->data);
  }
  
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

array_t*
array_push_batch(array_t *array, void *data, int n) {
  ensure_array_size(array, n);

  void* dst = ARRAY_GET_ELEM(array, array->size);
  memcpy(dst, data, array->elem_size * n);

  array->size += n;

  return array;
}

void*
array_pop_back(array_t *array) {
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
  ASSERT(to <= array->size && from < to);

  char *start = (char*)array->data + from * array->elem_size;
  char *end   = (char*)array->data + to   * array->elem_size;
  if (array->free) {
    char *p = start;
    do {
      array->free(p);
      p += array->elem_size;
    } while (p < end);
  }
  size_t remain = array->size - to;
  if (remain > 0) {
    memmove(start, end, remain * array->elem_size);
  }
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
array_append_array(array_t *array, const array_t *a) {
  ASSERT(array->elem_size == a->elem_size);

  size_t a_size = a->size;
  ensure_array_size(array, a_size);
  
  size_t index = array->size;
  char *end   = (char*)array->data + index * array->elem_size;

  memmove(end, a->data, a_size * array->elem_size);

  array->size += a_size;
}

void 
__array_assign(array_t *array, void* from, size_t new_size) {  
  size_t a_size = new_size / array->elem_size;
  ensure_array_size(array, a_size);

  char *start = (char*)array->data;
  memmove(start, from, new_size);

  array->size = a_size;
}

void 
array_copy(array_t *array, array_t *from) {
  ASSERT(array->elem_size == from->elem_size);
  size_t a_size = array_size(from);  
  char *start_from   = (char*)from->data;

  __array_assign(array, start_from, a_size * array->elem_size);
}

void 
array_assign_array(array_t *array, array_t* a, size_t from, size_t to) {
  ASSERT(array->elem_size == a->elem_size);
  ASSERT(to <= a->size && from < to);

  size_t a_size = to - from;
  char *start_from   = (char*)a->data + from * array->elem_size;

  __array_assign(array, start_from, a_size * array->elem_size);
}

void
array_assign(array_t *array, void *data) {
  array_clear(array);
  array_push(array, data);
}

bool 
array_equal(array_t *a1, array_t *a2) {
  ASSERT(a1->elem_size == a2->elem_size);

  if (a1->size != a2->size) {
    return false;
  }

  size_t i;
  for (i = 0; i < a1->size; i++) {
    void *p1 = array_get(a1, i);
    void *p2 = array_get(a2, i);
    if (a1->cmp != NULL) {
      if (!a1->cmp(p1, p2)) {
        return false;
      }
    } else if (memcmp(p1, p2, a1->elem_size) != 0) {
      return false;
    }
  }

  return true;
}