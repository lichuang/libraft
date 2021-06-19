/*
 * Copyright (C) lichuang
 */

#include "base/logger.h"
#include "core/read_only.h"

namespace libraft {

readOnly::readOnly(ReadOnlyOption option)
  : option_(option) {
}

// addRequest adds a read only reuqest into readonly struct.
// `index` is the commit index of the raft state machine when it received
// the read only request.
// `m` is the original read only request message from the local or remote node.
void readOnly::addRequest(uint64_t index, Message *msg) {
  string ctx = msg->entries(0).data();
  if (pendingReadIndex_.find(ctx) != pendingReadIndex_.end()) {
    return;
  }
  pendingReadIndex_[ctx] = new readIndexStatus(index, msg);
  readIndexQueue_.push_back(ctx);
}

// recvAck notifies the readonly struct that the raft state machine received
// an acknowledgment of the heartbeat that attached with the read only request
// context.
int readOnly::recvAck(const Message& msg) {
  map<string, readIndexStatus*>::iterator iter = pendingReadIndex_.find(msg.context());
  if (iter == pendingReadIndex_.end()) {
    return 0;
  }

  readIndexStatus* rs = iter->second;
  rs->acks_[msg.from()] = true; 
  return rs->acks_.size() + 1;
}

// advance advances the read only request queue kept by the readonly struct.
// It dequeues the requests until it finds the read only request that has
// the same context as the given `m`.
void readOnly::advance(const Message& msg, vector<readIndexStatus*> *rss) {
  size_t i;
  bool found = false;
  string ctx = msg.context();

  for (i = 0; i < readIndexQueue_.size(); ++i) {
    map<string, readIndexStatus*>::iterator iter = pendingReadIndex_.find(ctx);
    if (iter == pendingReadIndex_.end()) {
      Fatalf("cannot find corresponding read state from pending map");
    }

    readIndexStatus* rs = iter->second;
    rss->push_back(rs);
    if (ctx == readIndexQueue_[i]) {
      found = true;
      break;
    }
  }

  if (found) {
    ++i;
    readIndexQueue_.erase(readIndexQueue_.begin(), readIndexQueue_.begin() + i);
    for (i = 0; i < rss->size(); ++i) {
      pendingReadIndex_.erase((*rss)[i]->req_->entries(0).data());
    }
  }
}

// lastPendingRequestCtx returns the context of the last pending read only
// request in readonly struct.
string readOnly::lastPendingRequestCtx() {
  if (readIndexQueue_.size() == 0) {
    return "";
  }

  return readIndexQueue_[readIndexQueue_.size() - 1];
}

}; // namespace libraft