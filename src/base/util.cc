/*
 * Copyright (C) lichuang
 */

#include <string>
#include "base/default_logger.h"
#include "base/util.h"

using namespace std;
namespace libraft {
const char* GetErrorString(int err) {
  return "";
}

bool isDeepEqualSnapshot(const Snapshot *s1, const Snapshot *s2) {
  if (s1 == NULL || s2 == NULL) {
    return false;
  }

  if (s1->metadata().index() != s2->metadata().index()) {
    return false;
  }
  if (s1->metadata().term() != s2->metadata().term()) {
    return false;
  }
  if (s1->data() != s2->data()) {
    return false;
  }

  return true;
}

bool isDeepEqualEntry(const Entry& ent1, const Entry& ent2) {
  if (ent1.type() != ent2.type()) {
    return false;
  }
  if (ent1.term() != ent2.term()) {
    return false;
  }
  if (ent1.index() != ent2.index()) {
    return false;
  }
  if (ent1.data() != ent2.data()) {
    return false;
  }
  return true;
}

bool isDeepEqualEntries(const EntryVec& ents1, const EntryVec& ents2) {
  if (ents1.size() != ents2.size()) {
    return false;
  }
  size_t i;
  for (i = 0; i < ents1.size(); ++i) {
    if (!isDeepEqualEntry(ents1[i], ents2[i])) {
      return false;
    }
  }
  return true;
}

bool isDeepEqualNodes(const vector<uint64_t>& ns1, const vector<uint64_t>& ns2) {
  if (ns1.size() != ns2.size()) {
    return false;
  }
  size_t i;
  for (i = 0; i < ns1.size(); ++i) {
    if (ns1[i] != ns2[i]) {
      return false;
    }
  }
  return true;
}

void limitSize(uint64_t maxSize, EntryVec *entries) {
  if (entries->empty()) {
    return;
  }

  int limit;
  int num = entries->size();
  uint64_t size = (*entries)[0].ByteSizeLong();
  for (limit = 1; limit < num; ++limit) {
    size += (*entries)[limit].ByteSizeLong();
    if (size > maxSize) {
      break;
    }
  }

  entries->erase(entries->begin() + limit, entries->end());
}

bool isLoaclMessage(const MessageType type) {
  return (type == MsgHup          ||
          type == MsgBeat         ||
          type == MsgUnreachable  ||
          type == MsgSnapStatus   ||
          type == MsgCheckQuorum);
}

bool isResponseMessage(const MessageType type) {
  return (type == MsgAppResp        ||
          type == MsgVoteResp       ||
          type == MsgHeartbeatResp  ||
          type == MsgUnreachable    ||
          type == MsgPreVoteResp);
}

bool isHardStateEqual(const HardState& h1, const HardState& h2) {
  return h1.term() == h2.term() &&
         h1.vote() == h2.vote() &&
         h1.commit() == h2.commit();
}

bool isSoftStateEqual(const SoftState& s1, const SoftState& s2) {
  if (s1.leader != s2.leader) {
    return false;
  }

  return s1.state == s2.state;
}

bool isEmptySnapshot(const Snapshot* snapshot) {
  if (snapshot == NULL) {
    return true;
  }
  return snapshot->metadata().index() == 0;
}

bool isDeepEqualReadStates(const vector<ReadState*>& rs1, const vector<ReadState*>& rs2) {
  if (rs1.size() != rs2.size()) {
    return false;
  }
  size_t i;
  for (i = 0; i < rs1.size(); ++i) {
    ReadState* r1 = rs1[i];
    ReadState* r2 = rs2[i];
    if (r1->index_ != r2->index_) {
      return false;
    }
    if (r1->requestCtx_ != r2->requestCtx_) {
      return false;
    }
  }

  return true;
}

bool isDeepEqualMessage(const Message& msg1, const Message& msg2) {
  if (msg1.from() != msg2.from()) {
    return false;
  }
  if (msg1.to() != msg2.to()) {
    return false;
  }
  if (msg1.type() != msg2.type()) {
    return false;
  }

  if (msg1.entries_size() != msg2.entries_size()) {
    return false;
  }
  
  int i;
  for (i = 0; i < msg1.entries_size(); ++i) {
    if (!isDeepEqualEntry(msg1.entries(i), msg2.entries(i))) {
      return false;
    }
  }
  return true;
}

int numOfPendingConf(const EntryVec& entries) {
  size_t i;
  int n = 0;
  for (i = 0; i < entries.size(); ++i) {
    if (entries[i].type() == EntryConfChange) {
      ++n;
    }
  }

  return n;
}

MessageType voteRespMsgType(int t) {
  if (t == MsgVote) {
    return MsgVoteResp;
  }
  return MsgPreVoteResp;
}

string joinStrings(const vector<string>& strs, const string &sep) {
  string ret = "";
  size_t i;
  for (i = 0; i < strs.size(); ++i) {
    if (ret.length() > 0) {
      ret += sep;
    }
    ret += strs[i];
  }

  return ret;
}
}; // namespace libraft