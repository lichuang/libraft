/*
 * Copyright (C) lichuang
 */

#include <string.h>
#include <gtest/gtest.h>
#include "base/buffer.h"

using namespace libraft;

TEST(BufferTest, BufferTest) {
  Buffer buf;

  EXPECT_EQ(buf.Length(), (size_t)0);
  EXPECT_EQ(buf.WritableBytes(), kPacketSize);

  string str = "hello";
  buf.AppendString(str);
  EXPECT_EQ(buf.Length(), static_cast<size_t>(str.length()));
  EXPECT_EQ(buf.WritableBytes(), static_cast<size_t>(kPacketSize - str.length()));
}

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}