/*
 * Copyright (C) lichuang
 */

#pragma once

#include <stdint.h>
#include <vector>
#include "base/define.h"
#include "raft/protobuf/raft.pb.h"

using namespace std;

namespace libraft {

struct LogId {
  LogId() : index(0), term(0) {}
  LogId(int64_t index_, int64_t term_) : index(index_), term(term_) {}
  uint64_t index;
  int64_t term;
};

struct LogEntry {
public:
  EntryType type; // log type
  LogId id;
  std::vector<PeerId>* peers; // peers
  std::vector<PeerId>* old_peers; // peers

  LogEntry();

private:
  DISALLOW_COPY_AND_ASSIGN(LogEntry);

  virtual ~LogEntry();
};

};