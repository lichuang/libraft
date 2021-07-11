/*
 * Copyright (C) lichuang
 */

#ifndef __LIBRAFT_IO_H__
#define __LIBRAFT_IO_H__

namespace libraft {
class IOReader {
public:
  virtual ~IOReader() {}

  // read at least size bytes data, return actual read size and error code
  virtual size_t Read(char* buf, size_t size, int* err) = 0;
};

class IOWriter {
public:
  virtual ~IOWriter() {}

  virtual size_t Write(const char* buf, size_t size, int* err) = 0;
};

};

#endif // __LIBRAFT_IO_H__
