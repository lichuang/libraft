#ifndef __READ_ONLY_H__
#define __READ_ONLY_H__

#include "core/raft.h"
#include <map>
#include <string>

using namespace std;

struct readIndexStatus {
  uint64_t index_;
  Message *req_;
  map<uint64_t, bool> acks_;

  readIndexStatus(uint64_t index, Message *msg)
    : index_(index),
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

#endif  // __READ_ONLY_H__
