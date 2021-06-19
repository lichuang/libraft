/*
 * Copyright (C) lichuang
 */

#ifndef __LIBRAFT_READ_ONLY_H__
#define __LIBRAFT_READ_ONLY_H__

#include <map>
#include <string>
#include "core/raft.h"

using namespace std;
namespace libraft {

struct readIndexStatus {
  uint64_t index;
  Message *req_;
  map<uint64_t, bool> acks_;

  readIndexStatus(uint64_t index, Message *msg)
    : index(index),
      req_(msg) {
  }
};

struct readOnly {
  ReadOnlyOption option_;
  map<string, readIndexStatus*> pendingReadIndex_;
  vector<string> readIndexQueue_;
  Logger *logger_;

  readOnly(ReadOnlyOption option, Logger *logger);
  void addRequest(uint64_t index, Message *msg);
  int recvAck(const Message& msg);
  void advance(const Message& msg, vector<readIndexStatus*>* rss);
  string lastPendingRequestCtx();
};

}; // namespace libraft

#endif  // __LIBRAFT_READ_ONLY_H__
