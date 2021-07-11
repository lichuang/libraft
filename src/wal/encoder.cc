/*
 * Copyright (C) lichuang
 */

#include "base/file.h"
#include "base/crc32c.h"
#include "wal/encoder.h"

namespace libraft {

encoder*
newEncoder(File* file, uint32_t prevCrc, int32_t pageOffset) {
  encoder* ec = new encoder();
  return ec;
}

int 
encoder::encode(Record* rec) {
  locker.Lock();
  string data;
  rec->set_crc(Value(rec->data().c_str(), rec->data().size()));
  
  rec->SerializeToString(&data);
  
  uint64_t lenField;
  int64_t padBytes;
  encodeFrameSize(data.length(), &lenField, &padBytes);
  int err;
  err = file->WriteUint64(lenField);
  if (err != kOK) {
    return err;
  }

  if (padBytes != 0) {
    data += string('\0', padBytes);
  }

  return file->Write(data);
}

void 
encoder::encodeFrameSize(int64_t dataBytes, uint64_t* lenField, int64_t* padBytes) {
  *lenField = uint64_t(dataBytes);
  // force 8 byte alignment so length never gets a torn write
  *padBytes = (8 - (dataBytes % 8)) % 8;
  if (*padBytes != 0) {
    *lenField |= uint64_t(0x80 | *padBytes) << 56;
  }
}

};  // namespace libraft