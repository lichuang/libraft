/*
 * Copyright (C) lichuang
 */

#ifndef __LIBRAFT_DECODER_H__
#define __LIBRAFT_DECODER_H__

#include <vector>
#include "proto/record.pb.h"
#include "base/mutex.h"

using namespace std;
using namespace walpb;

namespace libraft {

class IOBuffer;

struct decoder {
  vector<IOBuffer*> io_buffers;

  // lastValidOff file offset following the last valid decoded record
  int64_t lastValidOff;

  uint32_t crc32;

  Locker locker;

  decoder(const vector<IOBuffer*>& buffer);
  ~decoder();

  int decode(Record* rec);
  int decodeRecord(Record* rec);
  void decodeFrameSize(int64_t len, int64_t* recBytes, int64_t* padBytes);

  // isTornEntry determines whether the last entry of the WAL was partially written
  // and corrupted because of a torn write.  
  bool isTornEntry(const char* data, uint32_t len);
};

//extern decoder* newDecoder();
extern decoder* newDecoder(const vector<IOBuffer*>& buffer);

}; // namespace libraft

#endif // __LIBRAFT_DECODER_H__