/*
 * Copyright (C) lichuang
 */

#ifndef __LIBRAFT_FILE_H__
#define __LIBRAFT_FILE_H__

#include <string>
#include "base/io_error.h"

using namespace std;

namespace libraft {


class File {
public:
  File(const string& path);

  int ReadInt64(int64_t* ret);
  int WriteUint64(uint64_t n);

  int ReadFull(char* data);
  int Write(const string& data);

  int Flush();
};

}; // namespace libraft

#endif  // __LIBRAFT_FILE_H__