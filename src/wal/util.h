/*
 * Copyright (C) lichuang
 */

#ifndef __LIBRAFT_UTIL_H__
#define __LIBRAFT_UTIL_H__

#include <string>
#include <vector>

using namespace std;

namespace libraft {

class FileSystemAdaptor;

extern string walName(uint64_t seq, uint64_t index);

extern bool readWalNames(FileSystemAdaptor*, const string& dir, vector<string>* names);

// searchIndex returns the last array index of names whose raft index section is
// equal to or smaller than the given index.
// The given names MUST be sorted.
int searchIndex(const vector<string>& names, uint64_t index);

// names should have been sorted based on sequence number.
// isValidSeq checks whether seq increases continuously.
bool isValidSeq(const vector<string>& names, int index);

};

#endif /* __LIBRAFT_UTIL_H__ */