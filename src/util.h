#ifndef __UTIL_H__
#define __UTIL_H__

#include "libraft.h"

void limitSize(uint64_t maxSize, EntryVec *entries);

bool isDeepEqualSnapshot(const Snapshot *s1, const Snapshot *s2);
bool isDeepEqualEntries(const EntryVec& ents1, const EntryVec& ents2);

#endif  // __UTIL_H__
