#include <string>
#include "util.h"

using namespace std;

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
  int i;
  for (i = 0; i < ents1.size(); ++i) {
    if (!isDeepEqualEntry(ents1[i], ents2[i])) {
      return false;
    }
  }
  return true;
}

void limitSize(uint64_t maxSize, EntryVec *entries) {
  uint64_t num = (uint64_t)entries->size();

  if (num == 0) {
    return;
  }

  int limit;
  uint64_t size = (*entries)[0].ByteSize();
  for (limit = 1; limit < size; ++limit) {
    size += (*entries)[limit].ByteSize();
    if (size > maxSize) {
      break;
    }
  }

  entries->erase(entries->begin() + limit, entries->end());
}

bool isHardStateEqual(const HardState& h1, const HardState& h2) {
  return h1.term() == h2.term() &&
         h1.vote() == h2.vote() &&
         h1.commit() == h2.commit();
}

bool isEmptySnapshot(const Snapshot* snapshot) {
  return snapshot->metadata().index() == 0;
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

MessageType voteRespMsgType(MessageType t) {
  if (t == MsgVote) {
    return MsgVoteResp;
  }
  if (t == MsgPreVote) {
    return MsgPreVoteResp;
  }
}

const char* msgTypeString(int type) {
  return "msg";
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
