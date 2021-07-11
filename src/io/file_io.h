/*
 * Copyright (C) lichuang
 */

#ifndef __LIBRAFT_FILE_IO_H__
#define __LIBRAFT_FILE_IO_H__

#include "io/io.h"

namespace libraft {

class File;

class FileIO : public IOReader,
               public IOWriter {
public:
  virtual ~FileIO() {}

  virtual size_t Read(char* buf, size_t size, int* err);
  virtual size_t Write(const char* buf, size_t size, int* err);

private:
  File* file_;
};

};

#endif  // __LIBRAFT_FILE_IO_H__
