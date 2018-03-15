#include <gtest/gtest.h>
#include "libraft.h"
#include "util.h"
#include "progress.h"
#include "default_logger.h"

bool deepEqualInflights(const inflights& in1, const inflights& in2) {
  if (in1.start_ != in2.start_) {
    return false;
  }
  if (in1.count_ != in2.count_) {
    return false;
  }
  if (in1.size_ != in2.size_) {
    return false;
  }
  if (in1.buffer_.size() != in2.buffer_.size()) {
    return false;
  }
  int i = 0;
  for (i = 0; i < in1.buffer_.size(); ++i) {
    if (in1.buffer_[i] != in2.buffer_[i]) {
      return false;
    }
  }

  return true;
}

TEST(progressTests, TestInflightsAdd) {
  inflights ins(10, &kDefaultLogger);
  int i;

  for (i = 0; i < 5; ++i) {
    ins.add(i);
  }

  {
    inflights wantIns(10, &kDefaultLogger);
    wantIns.start_ = 0;
    wantIns.count_ = 5;
    wantIns.size_  = 10;
    wantIns.buffer_.push_back(0);
    wantIns.buffer_.push_back(1);
    wantIns.buffer_.push_back(2);
    wantIns.buffer_.push_back(3);
    wantIns.buffer_.push_back(4);
    wantIns.buffer_.push_back(0);
    wantIns.buffer_.push_back(0);
    wantIns.buffer_.push_back(0);
    wantIns.buffer_.push_back(0);
    wantIns.buffer_.push_back(0);
    // ↓------------
    // 0, 1, 2, 3, 4, 0, 0, 0, 0, 0
    EXPECT_EQ(true, deepEqualInflights(ins, wantIns));
  }

  for (i = 5; i < 10; ++i) {
    ins.add(i);
  }

  {
    inflights wantIns(10, &kDefaultLogger);
    wantIns.start_ = 0;
    wantIns.count_ = 10;
    wantIns.size_  = 10;
    wantIns.buffer_.push_back(0);
    wantIns.buffer_.push_back(1);
    wantIns.buffer_.push_back(2);
    wantIns.buffer_.push_back(3);
    wantIns.buffer_.push_back(4);
    wantIns.buffer_.push_back(5);
    wantIns.buffer_.push_back(6);
    wantIns.buffer_.push_back(7);
    wantIns.buffer_.push_back(8);
    wantIns.buffer_.push_back(9);
    // ↓--------------------------
    // 0, 1, 2, 3, 4, 5, 6, 7, 8, 9
    EXPECT_EQ(true, deepEqualInflights(ins, wantIns));
  }

  // rotating case
  inflights ins2(10, &kDefaultLogger);
  ins2.start_ = 5;
  ins2.size_ = 10;

  return;
  for (i = 0; i < 5; ++i) {
    ins2.add(i);
  }
  {
    inflights wantIns(10, &kDefaultLogger);
    wantIns.start_ = 5;
    wantIns.count_ = 5;
    wantIns.size_  = 10;
    wantIns.buffer_.push_back(0);
    wantIns.buffer_.push_back(0);
    wantIns.buffer_.push_back(0);
    wantIns.buffer_.push_back(0);
    wantIns.buffer_.push_back(0);
    wantIns.buffer_.push_back(0);
    wantIns.buffer_.push_back(1);
    wantIns.buffer_.push_back(2);
    wantIns.buffer_.push_back(3);
    wantIns.buffer_.push_back(4);
    //                ↓------------
    // 0, 0, 0, 0, 0, 0, 1, 2, 3, 4
    EXPECT_EQ(true, deepEqualInflights(ins2, wantIns));
  }
}
