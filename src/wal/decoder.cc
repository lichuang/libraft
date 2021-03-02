/*
 * Copyright (C) lichuang
 */

#include "base/crc32.h"
#include "base/io_buffer.h"
#include "base/file.h"
#include "wal/decoder.h"
#include "wal/wal.h"

namespace libraft {

const uint32_t kMinSectorSize = 512;

int 
decoder::decode(Record* rec) {
  //rec->Reset();
  locker.Lock();

  return decodeRecord(rec);
}

int 
decoder::decodeRecord(Record* rec) {
  if (io_buffers.size() == 0) {
    return kEOF;
  }

  int err;
  int64_t lenField = 0;
  int64_t recBytes, padBytes;

  err = io_buffers[0]->ReadInt64(&lenField);
  if (err == kEOF || lenField == 0) {
    // hit end of file or preallocated space
    IOBuffer* buffer = io_buffers[0];
    delete buffer;
    io_buffers.erase(io_buffers.begin());
    if (io_buffers.size() == 0) {
      return kEOF;
    }

    lastValidOff = 0;
    return decodeRecord(rec);
  }

  if (err != kOK) {
    return err;
  }

  decodeFrameSize(lenField, &recBytes, &padBytes);

  char *data = new char[recBytes + padBytes];
  err = io_buffers[0]->ReadFull(data);
  if (err != kOK) {
		// ReadFull returns io.kEOF only if no bytes were read
		// the decoder should treat this as an kErrUnexpectedEOF instead.
    if (err == kEOF) {
      err = kErrUnexpectedEOF;
      goto out;
    }

    goto out;
  }

  if (!rec->ParseFromString(string(data, recBytes))) {
    if (isTornEntry(data, recBytes + padBytes)) {
      err = kErrUnexpectedEOF;      
    } else {
      err = kEOF;
    }
    goto out;
  }

  // skip crc checking if the record type is crcType
  if (rec->type() != crcType) {
    crc32 = Value(rec->data().c_str(),rec->data().size());
    if (crc32 != rec->crc()) {
      if (isTornEntry(rec->data().c_str(),rec->data().size())) {
        return kErrUnexpectedEOF;
      }      
    }
    return kEOF;
  }

  // record decoded as valid; point last valid offset to end of record
  lastValidOff += recBytes + padBytes + 8;

out:
  delete [] data;
  return err;
}

void 
decoder::decodeFrameSize(int64_t lenField, int64_t* recBytes, int64_t* padBytes) {
  // the record size is stored in the lower 56 bits of the 64-bit length
  *recBytes = int64_t(uint64_t(lenField) & ((((uint64_t)(0xff) << 56))));

  // non-zero padding is indicated by set MSb / a negative length
  if (lenField < 0) {
    // padding is stored in lower 3 bits of length MSB
    *padBytes = int64_t(((uint64_t)lenField >> 56) & 0x7);
  }
}

// isTornEntry determines whether the last entry of the WAL was partially written
// and corrupted because of a torn write.
struct chunk {
  const char* buf;
  int32_t len;
};
bool 
decoder::isTornEntry(const char* data, uint32_t len) {
  if (io_buffers.size() != 1) {
    return false;
  }

  int64_t fileOff = lastValidOff + 8;
  int64_t curOff = 0;

  vector<chunk> chunks;

  // split data on sector boundaries
  for (; curOff < len;) {
    int32_t chunkLen = kMinSectorSize - (fileOff % kMinSectorSize);
    if (chunkLen > len - curOff) {
      chunkLen = len - curOff;
    }

    chunks.push_back((chunk){.buf = data + curOff, .len = chunkLen});
    fileOff += chunkLen;
    curOff += chunkLen;
  }

  // if any data for a sector chunk is all 0, it's a torn write
  uint32_t i;
  for (i = 0; i < chunks.size(); i++) {
    chunk& c = chunks[i];
    int j;
    for (j = 0; j < c.len; j++) {
      if (c.buf[j] != '\0') {
        return false;
      }
    }
  }

  return true;
}

decoder::decoder(const vector<IOBuffer*>& buffer) {
  uint32_t i;
  for (i = 0; i < buffer.size(); ++i) {
    io_buffers.push_back(buffer[i]);
  }

  lastValidOff = 0;
  crc32 = 0;
}

decoder::~decoder() {

}

decoder* 
newDecoder(const vector<IOBuffer*>& buffer) {
  return new decoder(buffer);
}

};  // namespace libraft