/*
 * Copyright (C) lichuang
 */

#include <string>
#include <gtest/gtest.h>
#include "base/io_buffer.h"

using namespace libraft;

TEST(iobufferTests, TestMemoryBuffer) {
  IOBuffer* mb = newMemoryBuffer();

  mb->WriteUint64(1024);
  int64_t ret = 0;
  mb->ReadInt64(&ret);

  ASSERT_EQ(ret, 1024);

  delete mb;
}

TEST(iobufferTests, TestMemoryBufferWithString) {
  string test = string("\b\xef\xfd\x02");
  IOBuffer* mb = newMemoryBufferWithString(test);
  char tmp[20] = {'\0'};
  int err;
  int size = mb->ReadFull(tmp, 20, &err);

  ASSERT_EQ((int)test.size(), size);
  ASSERT_EQ(0, strncmp(tmp, test.c_str(), size));

  delete mb;
}
