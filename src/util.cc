#include <string>
#include "util.h"

using namespace std;

const char* GetErrorString(int err) {
  return "";
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
