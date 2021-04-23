/*
 * Copyright (C) lichuang
 */

#include "ctest.h"
#include "util/array.h"

CTEST(array_test, test_push_pop) {
  array_t *test = array_create(sizeof(int));

  int i;
  for (i = 0; i < 50; ++i) {
    array_push(test, &i);
  }

  ASSERT_EQUAL(array_size(test), i);

  --i;
  for (; i >= 0; --i) {
    int* j = array_pop(test);
    ASSERT_EQUAL(*j, i);
  }

  array_destroy(test);
}

CTEST(array_test, test_erase) {
  array_t *test = array_create(sizeof(int));

  int i;
  for (i = 0; i < 50; ++i) {
    array_push(test, &i);
  }

  // before erase
  int* j = array_get(test, 1);
  ASSERT_EQUAL(*j, 1);
  ASSERT_EQUAL(array_size(test), 50);

  array_erase(test, 1, 10);
  
  // after erase
  j = array_get(test, 1);
  ASSERT_EQUAL(*j, 10);
  ASSERT_EQUAL(array_size(test), 41);

  array_destroy(test);
}

CTEST(array_test, test_insert) {
  array_t *test1 = array_create(sizeof(int));
  array_t *test2 = array_create(sizeof(int));

  int i;
  for (i = 0; i < 5; ++i) {
    array_push(test1, &i);
  }

  for (i = 10; i < 15; ++i) {
    array_push(test2, &i);
  }

  // before insert
  size_t index = 2;
  int* j = array_get(test1, index);
  ASSERT_EQUAL(*j, index);
  ASSERT_EQUAL(array_size(test1), 5);

  array_insert_array(test1, 2, test2);

  // after insert
  for (i = index; i < array_size(test2); ++i) {
    int *n = array_get(test1, i);
    int *m = array_get(test2, i - index);
    ASSERT_EQUAL(*n, *m);
  }
  ASSERT_EQUAL(array_size(test1), 10);

  array_destroy(test1);
  array_destroy(test2);
}

CTEST(array_test, test_copy) {
  array_t *test1 = array_create(sizeof(int));
  array_t *test2 = array_create(sizeof(int));

  int i;
  for (i = 0; i < 5; ++i) {
    array_push(test1, &i);
  }

  for (i = 10; i < 25; ++i) {
    array_push(test2, &i);
  }

  // before copy
  size_t index = 2;
  int* j = array_get(test1, index);
  ASSERT_EQUAL(*j, index);
  ASSERT_EQUAL(array_size(test1), 5);

  array_copy(test1, test2);

  // after copy
  j = array_get(test1, index);
  ASSERT_EQUAL(*j, 12);
  ASSERT_EQUAL(array_size(test1), array_size(test2));

  array_destroy(test1);
  array_destroy(test2);
}