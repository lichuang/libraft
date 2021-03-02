/*
 * Copyright (C) lichuang
 */

#ifndef __LIBRAFT_ENCODER_H__
#define __LIBRAFT_ENCODER_H__

#include <vector>
#include "proto/record.pb.h"
#include "base/mutex.h"

using namespace std;
using namespace walpb;

namespace libraft {

class File;

struct encoder {
  char *buf;
  Locker locker;
  uint32_t crc;
  File* file;
  
  int encode(Record* rec);

  void encodeFrameSize(int64_t dataBytes, uint64_t* lenField, int64_t* padBytes);
};

extern encoder* newEncoder(File* file, uint32_t prevCrc, int32_t pageOffset);

}; // namespace libraft

#endif // __LIBRAFT_ENCODER_H__