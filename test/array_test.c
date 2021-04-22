/*
 * Copyright (C) lichuang
 */

#include "ctest.h"
#include "util/array.h"

CTEST(array_test, test_push_pop) {
  array_t test;
  array_init(&test, sizeof(int));

  int i;
  for (i = 0; i < 50; ++i) {
    array_push(&test, &i);
  }

  --i;
  for (; i >= 0; --i) {
    int* j = array_pop(&test);
    ASSERT_EQUAL(*j, i);
  }

  array_destroy(&test);
}