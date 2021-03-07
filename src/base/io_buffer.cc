/*
 * Copyright (C) lichuang
 */

#include <string.h>
#include "base/io_buffer.h"
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

  void AdvanceWrite(uint32_t wpos) { write_pos += wpos;}

  char* WritePos() { return buffer + write_pos; }

  void AdvanceRead(uint32_t rpos) { read_pos += rpos;}

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

IOBuffer::IOBuffer()
  : head_(new Block()),
    read_block_(head_),
    write_block_(head_) {

}

IOBuffer::~IOBuffer() {
  delete head_;
}

void
IOBuffer::ensureMemory(uint32_t size) {
  if (write_block_ && write_block_->WriteSize() > size) {
    return;
  }

  Block* block = write_block_;
  while (size > 0) {
    block->next = new Block();
    block = block->next;
    size -= kBlockSize;    
  }
}

int 
IOBuffer::ReadFull(char* data, uint32_t bufsize, int* err) {
  *err = kOK;
  uint32_t offset = 0, size, total = 0;
  while (read_block_) {  
    size = read_block_->ReadSize();  
    if (size == 0) {
      goto out;
    }
    memcpy(data + offset, read_block_->ReadPos(), size);
    read_block_->AdvanceRead(size);
    offset += size;
    total += size;
    read_block_ = read_block_->next;    
  }

out:
  if (total < bufsize) {
    *err = (total == 0) ? kEOF : kErrUnexpectedEOF;
  }
  
  return total;
}

class MemoryBuffer : public IOBuffer {
public:
  MemoryBuffer();
  MemoryBuffer(const string& data);
  virtual ~MemoryBuffer();
  
  int ReadInt64(int64_t* ret);
  int WriteUint64(uint64_t n);  

  void Append(const string& data); 
private:
  
};

MemoryBuffer::MemoryBuffer()
  : IOBuffer() {  
}

MemoryBuffer::MemoryBuffer(const string& data)
  : IOBuffer() {  
  Append(data);
}

MemoryBuffer::~MemoryBuffer() {

}

void
MemoryBuffer::Append(const string& data) {
  ensureMemory(data.size());
  uint32_t size = data.size();
  uint32_t offset = 0;
  do {
    if (write_block_->WriteSize() == 0) {
      write_block_ = write_block_->next;
    }
    uint32_t wsize = write_block_->WriteSize() >= size ? size : write_block_->WriteSize();
    memcpy(write_block_->WritePos(), data.c_str() + offset, wsize);
    write_block_->AdvanceWrite(wsize);
    size -= wsize;
  } while (size > 0);
}

int 
MemoryBuffer::ReadInt64(int64_t* ret) {
  memcpy(ret, read_block_->ReadPos(), sizeof(int64_t));
  read_block_->AdvanceRead(sizeof(int64_t));
  return 0;
}

int 
MemoryBuffer::WriteUint64(uint64_t n) {
  ensureMemory(sizeof(uint64_t));
  memcpy(&(write_block_->WritePos()[0]), &n, sizeof(uint64_t));
  write_block_->AdvanceWrite(sizeof(uint64_t));
  return 0;
}


IOBuffer* 
newMemoryBuffer() { 
  return new MemoryBuffer();
}

IOBuffer* 
newMemoryBufferWithString(const string& data) {
  return new MemoryBuffer(data);
}

};  // namespace libraft