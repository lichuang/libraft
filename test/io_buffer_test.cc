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
  IOBuffer* mb = newMemoryBufferWithString("test");
  char tmp[10];
  mb->ReadFull(tmp);

  ASSERT_EQ(0, strncmp(tmp, "test", 4));

  delete mb;
}
