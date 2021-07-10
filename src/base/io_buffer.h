/*
 * Copyright (C) lichuang
 */

#ifndef __LIBRAFT_IO_BUFFER_H__
#define __LIBRAFT_IO_BUFFER_H__

#include <string>

using namespace std;

namespace libraft {

struct Block;

class IOBuffer {
protected:
  IOBuffer();  

  void ensureMemory(uint32_t size);
public:
  virtual int ReadInt64(int64_t* ret) = 0;
  virtual int WriteUint64(uint64_t n) = 0;

  virtual void Append(const string& data) = 0; 

  virtual ~IOBuffer();

  int ReadFull(char* data, uint32_t size, int* err);
  /*
  
  int Write(const string& data);

  int Flush();
  */

protected:  
  Block *head_;
  Block* read_block_;
  Block* write_block_;
};

extern IOBuffer* newMemoryBuffer();
extern IOBuffer* newMemoryBufferWithString(const string& data);

};  // namespace libraft

#endif  // __LIBRAFT_IO_BUFFER_H__