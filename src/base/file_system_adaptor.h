/*
 * Copyright (C) lichuang
 */

#ifndef __LIBRAFT_FILE_SYSTEM_ADAPTOR_H__
#define __LIBRAFT_FILE_SYSTEM_ADAPTOR_H__

#include <string>

using namespace std;

namespace libraft {

class FileSystemAdaptor {
public:
  FileSystemAdaptor();
  virtual ~FileSystemAdaptor() {}

  // Determine whether a given path exists
  virtual bool PathExists(const string& path) = 0;

  // Determine whether a given directory exists
  virtual bool DirectoryExists(const string& dir) = 0;

  // Deletes the given path, whether it's a file or a directory.
  virtual bool DeleteFile(const string& path,bool recursive) = 0;

  // 
  virtual bool CreateDirectory(const string& path) = 0;

  virtual string Join(const string&dir, const string&name) = 0;
};

class PosixFileSystemAdaptor : public FileSystemAdaptor {
public:
  PosixFileSystemAdaptor();
  virtual ~PosixFileSystemAdaptor() {}

  virtual bool PathExists(const string& path);

  virtual bool DirectoryExists(const string& dir);

  virtual bool DeleteFile(const string& path, bool recursive);

  virtual bool CreateDirectory(const string& path);

  virtual string Join(const string&dir, const string&name);
};

}; // namespace libraft

#endif  // __LIBRAFT_FILE_SYSTEM_ADAPTOR_H__