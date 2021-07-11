/*
 * Copyright (C) lichuang
 */

#include "io/buffer_io_reader.h"
#include "io/io.h"
#include "base/io_error.h"

namespace libraft {
static const uint32_t kBlockSize = 1024;

struct Block {
  char buffer[kBlockSize];
  uint32_t read_pos;
  uint32_t write_pos;
  Block* next;

  uint32_t WriteSize() { return kBlockSize - write_pos; }

  uint32_t ReadSize() { 
    if (write_pos <= read_pos) {
      return 0;
    }
    return write_pos - read_pos; 
  }

  size_t Read(char* buf, size_t size) {
    memcpy(buf, ReadPos(), size);
    advanceRead(size);
    return size;
  }

  size_t ReadAll(char* buf, size_t size) {
    size_t readSize = ReadSize();
    if (size > readSize) {
      size = readSize;
    }
    memcpy(buf, ReadPos(), readSize);
    advanceRead(readSize);
    return readSize;
  }

  void AdvanceWrite(uint32_t wpos) { write_pos += wpos;}

  char* WritePos() { return buffer + write_pos; }

  void advanceRead(uint32_t rpos) { 
    read_pos += rpos;
  }

  char* ReadPos() { return buffer + read_pos; }
  
  Block()
    : read_pos(0), 
      write_pos(),
      next(NULL) {
  }

  ~Block() {
    if (next) {
      delete next;
    }
  }
};

BufferIOReader::BufferIOReader(IOReader* reader)
  : head_(new Block()),
    read_block_(head_),
    write_block_(head_),
    reader_(reader) {
}

BufferIOReader::~BufferIOReader() {
  delete head_;
  delete reader_;
}

void 
BufferIOReader::readAtLeast(size_t size) {
  int err;

  while (size >= 0) {
    if (write_block_->WriteSize() == 0) {
      write_block_->next = new Block();
      write_block_ = write_block_->next;
    }
    uint32_t wsize = write_block_->WriteSize() >= kBlockSize ? kBlockSize : write_block_->WriteSize();    
    size_t rsize = reader_->Read(write_block_->WritePos(), wsize, &err);
    write_block_->AdvanceWrite(rsize);
    size -= rsize;
    if (err != kOK) {
      break;
    }        
  }
}

size_t
BufferIOReader::Read(char* buf, size_t size, int* err) {
  // is the buffer has enough data?
  if (read_block_->ReadSize() >= size) {
    read_block_->Read(buf, size);
    *err = kOK;
    return size;
  }

  readAtLeast(size);
  size_t readSize = read_block_->ReadAll(buf, size);
  *err = (readSize == size) ? kOK : kEOF;
  return readSize;
}

int 
BufferIOReader::ReadInt64(int64_t* ret) {
  if (read_block_->ReadSize() >= sizeof(int64_t)) {
    read_block_->Read((char*)ret, sizeof(int64_t));
    return 0;
  }
  
  readAtLeast(sizeof(int64_t));
  if (read_block_->ReadSize() >= sizeof(int64_t)) {
    read_block_->Read((char*)ret, sizeof(int64_t));
    return 0;    
  }

  char tmp[sizeof(int64_t)];
  read_block_->Read(tmp, sizeof(int64_t));

  return 0;
}

};