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
