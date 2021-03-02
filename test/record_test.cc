/*
 * Copyright (C) lichuang
 */

#include <string>
#include <gtest/gtest.h>
#include "raft_test_util.h"
#include "base/crc32.h"
#include "base/io_buffer.h"
#include "proto/record.pb.h"
#include "wal/decoder.h"

using namespace std;
using namespace walpb;

const static string infoData = string("\b\xef\xfd\x02");
const static string infoRecord = string("\x0e\x00\x00\x00\x00\x00\x00\x00\b\x01\x10\x99\xb5\xe4\xd0\x03\x1a\x04") + infoData;

static inline Record
initRecord(int64_t type, uint32_t crc, const string& data) {
  Record record;
  record.set_type(type);
  record.set_crc(crc);
  record.set_data(data);

  return record;
}

TEST(recordTests, TestReadRecord) {
  char * badInfoRecord = new char[infoRecord.size()];
  strncpy(badInfoRecord, infoRecord.c_str(), infoRecord.size());
  badInfoRecord[infoRecord.size() - 1] = 'a';

  struct tmp {
    string data;
    Record wr;
    int we;
  } tests[] = {
    {
      .data = infoRecord,
      .wr = initRecord(1, Value(infoData.c_str(), infoData.size()), infoData),
      .we = 0,
    },
  };

  uint32_t i;
  for (i = 0; i < SIZEOF_ARRAY(tests); ++i) {
    tmp& tt = tests[i];
    Record record;
    vector<IOBuffer*> buf = {newMemoryBufferWithString(tt.data)};
    decoder* dec = newDecoder(buf);
    int err = dec->decode(&record);
    
    ASSERT_EQ(err, tt.we);

    delete dec;
  }

  delete [] badInfoRecord;
}