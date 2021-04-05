/*
 * Copyright (C) lichuang
 */

#ifndef __LIBRAFT_UTIL_H__
#define __LIBRAFT_UTIL_H__

#include "libraft.h"
#include "proto/record.pb.h"

using namespace walpb;

namespace libraft {

void limitSize(uint64_t maxSize, EntryVec *entries);

bool isDeepEqualNodes(const vector<uint64_t>& ns1, const vector<uint64_t>& ns2);
bool isDeepEqualSnapshot(const Snapshot *s1, const Snapshot *s2);
bool isDeepEqualEntries(const EntryVec& ents1, const EntryVec& ents2);
bool isDeepEqualReadStates(const vector<ReadState*>& rs1, const vector<ReadState*>& rs2);
bool isDeepEqualMessage(const Message& msg1, const Message& msg2);
bool isDeepEqualRecord(const Record& r1, const Record& r2);
bool isHardStateEqual(const HardState& h1, const HardState& h2);
bool isSoftStateEqual(const SoftState& s1, const SoftState& s2);
bool isEmptySnapshot(const Snapshot* snapshot);
int numOfPendingConf(const EntryVec& entries);
MessageType voteRespMsgType(int t);

bool isLoaclMessage(const MessageType type);
bool isResponseMessage(const MessageType type);

string entryStr(const Entry& entry);
string entryVecDebugString(const EntryVec& entries);

// string util
string joinStrings(const vector<string>& strs, const string &sep);

}; // namespace libraft

#endif  // __LIBRAFT_UTIL_H__
