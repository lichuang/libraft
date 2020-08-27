/*
 * Copyright (C) lichuang
 */

#include <string.h>
#include <gtest/gtest.h>
#include "base/buffer.h"

using namespace libraft;

TEST(BufferTest, BufferTest) {
  /*
  Buffer buf;

  EXPECT_EQ(buf.Length(), 0);
  EXPECT_EQ(buf.Available(), kInitBufferSize);

  string str = "hello";
  buf.AppendString(str);
  EXPECT_EQ(buf.Length(), static_cast<int>(str.length()));
  EXPECT_EQ(buf.Available(), static_cast<int>(kInitBufferSize - str.length()));

  buf.Reset();
  EXPECT_EQ(buf.Length(), 0);
  EXPECT_EQ(buf.Available(), kInitBufferSize); 
  */ 
}

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}