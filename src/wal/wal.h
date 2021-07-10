/*
 * Copyright (C) lichuang
 */

#ifndef __LIBRAFT_WAL_H__
#define __LIBRAFT_WAL_H__

#include <string>
#include "libraft.h"
#include "proto/record.pb.h"

using namespace std;
using namespace walpb;

namespace libraft {

class FileSystemAdaptor;
class Logger;

enum walDataType {
  metadataType = 1,
  entryType,
  stateType,
  crcType,
  snapshotType,
};

struct walOption {
  FileSystemAdaptor* fs;

  Logger *logger;
};

enum walErrorCode {
  kErrCRCMismatch = -100,
};

// WAL is a logical representation of the stable storage.
// WAL is either in read mode or append mode but not both.
// A newly created WAL is in append mode, and ready for appending records.
// A just opened WAL is in read mode, and ready for reading records.
// The WAL will be ready for appending after reading out all the previous records.
struct WAL {

};

// Create creates a WAL ready for appending records. The given metadata is
// recorded at the head of each WAL file, and can be retrieved with ReadAll.
extern WAL* CreateWAL(walOption*, const string& dir,const string& meta);

// OpenWAL opens the WAL at the given snap.
// The snap SHOULD have been previously saved to the WAL, or the following
// ReadAll will fail.
// The returned WAL is ready to read and the first record will be the one after
// the given snap. The WAL cannot be appended to before reading out all of its
// previous records.
extern WAL* OpenWAL(walOption*, const string& dir, const WalSnapshot&);

// OpenForRead only opens the wal files for read.
// Write on a read only wal panics.
extern WAL* OpenForRead(walOption*, const string& dir, const WalSnapshot&);

}; // namespace libraft

#endif // __LIBRAFT_WAL_H__