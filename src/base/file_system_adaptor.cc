/*
 * Copyright (C) lichuang
 */

#include <unistd.h>
#include <sys/stat.h>
#include "base/file_system_adaptor.h"

namespace libraft {
bool 
PosixFileSystemAdaptor::PathExists(const string& path) {
  return access(path.c_str(), F_OK) == 0;
}

bool 
PosixFileSystemAdaptor::DirectoryExists(const string& dir) {
  struct stat buf;
  if (::stat(dir.c_str(), &buf) == 0) {
    return S_ISDIR(buf.st_mode);
  }
  return false;
}

bool 
PosixFileSystemAdaptor::DeleteFile(const string& path, bool recursive) {
  return true;
}

};  // namespace libraft