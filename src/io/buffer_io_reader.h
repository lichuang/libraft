/*
 * Copyright (C) lichuang
 */

#ifndef __LIBRAFT_BUFFER_IO_READER_H__
#define __LIBRAFT_BUFFER_IO_READER_H__

#include <string>

using namespace std;

namespace libraft {

class IOReader;
struct Block;

class BufferIOReader {
public:
  BufferIOReader(IOReader* reader);

  virtual ~BufferIOReader();

  virtual size_t Read(char* buf, size_t size, int* err);

  int ReadInt64(int64_t* ret);

private:
  void readAtLeast(size_t size);

private:  
  Block *head_;
  Block* read_block_;
  Block* write_block_;
  IOReader* reader_;
};

};

#endif  // __LIBRAFT_BUFFER_IO_READER_H__