#ifndef __UTIL_H__
#define __UTIL_H__

#include "libraft.h"

void limitSize(uint64_t maxSize, EntryVec *entries);

bool isDeepEqualSnapshot(const Snapshot *s1, const Snapshot *s2);
bool isDeepEqualEntries(const EntryVec& ents1, const EntryVec& ents2);
bool isHardStateEqual(const HardState& h1, const HardState& h2);
bool isEmptySnapshot(const Snapshot* snapshot);
int numOfPendingConf(const EntryVec& entries);
MessageType voteRespMsgType(MessageType t);

const char* msgTypeString(int type);

// string util
string joinStrings(const vector<string>& strs, const string &sep);

#endif  // __UTIL_H__
