#include <gtest/gtest.h>
#include "libraft.h"
#include "util.h"
#include "raft.h"
#include "memory_storage.h"
#include "default_logger.h"
#include "progress.h"

TEST(raftTests, TestProgressBecomeProbe) {
  uint64_t match = 1;
  struct tmp {
    Progress p;
    uint64_t wnext;

    tmp(Progress p, uint64_t next)
      : p(p), wnext(next) {
    }
  };

  vector<tmp> tests;
  {
    Progress p(5, 256, &kDefaultLogger);
    p.state_ = ProgressStateReplicate;
    p.match_ = match;
    tests.push_back(tmp(p, 2));
  }
  // snapshot finish
  {
    Progress p(5, 256, &kDefaultLogger);
    p.state_ = ProgressStateSnapshot;
    p.match_ = match;
    p.pendingSnapshot_ = 10;
    tests.push_back(tmp(p, 11));
  }
  // snapshot failure
  {
    Progress p(5, 256, &kDefaultLogger);
    p.state_ = ProgressStateSnapshot;
    p.match_ = match;
    p.pendingSnapshot_ = 0;
    tests.push_back(tmp(p, 2));
  }

  int i;
  for (i = 0; i < tests.size(); ++i) {
    tmp &t = tests[i];
    t.p.becomeProbe();
    EXPECT_EQ(t.p.state_, ProgressStateProbe);
    EXPECT_EQ(t.p.match_, match);
    EXPECT_EQ(t.p.next_, t.wnext);
  }
}

TEST(raftTests, TestProgressBecomeReplicate) {
  Progress p(5, 256, &kDefaultLogger);
  p.state_ = ProgressStateProbe;
  p.match_ = 1;

  p.becomeReplicate();
  EXPECT_EQ(p.state_, ProgressStateReplicate);
  EXPECT_EQ(p.match_, 1);
  EXPECT_EQ(p.next_, p.match_ + 1);
}
