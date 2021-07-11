/*
 * Copyright (C) lichuang
 */

#include <string>
#include <gtest/gtest.h>
#include "raft_test_util.h"
#include "base/crc32c.h"
#include "base/io_error.h"
#include "base/io_buffer.h"
#include "base/util.h"
#include "proto/record.pb.h"
#include "wal/decoder.h"
#include "wal/wal.h"

using namespace std;
using namespace walpb;

typedef unsigned char byte;
typedef vector<byte> ByteVector;

const static byte infoDataBytes[] = "\b\xef\xfd\x02";
const static byte infoRecordBytes[] = "\x0e\x00\x00\x00\x00\x00\x00\x00\b\x01\x10\x99\xb5\xe4\xd0\x03\x1a\x04";

const static string infoData = string((char*)infoDataBytes, sizeof(infoDataBytes));
const static string infoRecord = string((char*)infoRecordBytes, sizeof(infoRecordBytes) - 1) + infoData;

static inline Record
initRecord(int64_t type, uint32_t crc, const string& data) {
  Record record;
  record.set_type(type);
  record.set_crc(crc);
  record.set_data(data);

  return record;
}

TEST(recordTests, TestReadRecord) {
  string badInfoRecord = string(infoRecord.c_str(), infoRecord.length() - 1);
  badInfoRecord[badInfoRecord.length() - 1] = 'a';

  struct tmp {
    string data;
    Record wr;
    int we;
  } tests[] = {
    {
      .data = infoRecord,
      .wr = initRecord(1, Value(infoData.c_str(), infoData.length() - 1), string(infoData.c_str(), infoData.length() - 1)),
      .we = 0,
    },
    {
      .data = "", .wr = Record(), .we = kEOF,
    },
    {
      .data = infoRecord.substr(0,8), .wr = Record(), .we = kErrUnexpectedEOF,
    },  
    {
      .data = infoRecord.substr(0,infoRecord.length() - infoData.length() - 8), .wr = Record(), .we = kErrUnexpectedEOF,
    },
    {
      .data = infoRecord.substr(0,infoRecord.length() - infoData.length()), .wr = Record(), .we = kErrUnexpectedEOF,
    }, 
    {
      .data = infoRecord.substr(0,infoRecord.length() - 8), .wr = Record(), .we = kErrUnexpectedEOF,
    },   
    {
      .data = badInfoRecord, .wr = initRecord(1,0,""), .we = kErrCRCMismatch,
    },       
  };

  uint32_t i;
  for (i = 0; i < SIZEOF_ARRAY(tests); ++i) {
    tmp& tt = tests[i];
    Record record;
    vector<IOBuffer*> buf = {newMemoryBufferWithString(tt.data)};
    Decoder* dec = newDecoder(buf);
    int err = dec->decode(&record);
    
    ASSERT_EQ(err, tt.we) << "i:" << i << tt.data.length();

    if (tt.we != kErrCRCMismatch)
      ASSERT_TRUE(isDeepEqualRecord(record, tt.wr)) << "i:" << i;

    delete dec;
  }
}

TEST(recordTests, TestWriteRecord) {
}