/*
 * Copyright (C) lichuang
 */

#include "base/file_system_adaptor.h"
#include "base/logger.h"
#include "wal/util.h"
#include "wal/wal.h"

namespace libraft {

static WAL* openAtIndex(walOption*, const string&, const WalSnapshot&, bool);

WAL* 
createWAL(walOption* option, const string& dir,const string& meta) {
  FileSystemAdaptor* fs = option->fs;

  if (fs->PathExists(dir)) {
    Errorf("wal dir %s exists",dir.c_str());
    return NULL;
  }

  // keep temporary wal directory so WAL initialization appears atomic
  string tmpdir = dir + ".tmp";
  if (fs->DirectoryExists(tmpdir) && !fs->DeleteFile(tmpdir, true)) {
    Errorf("delete wal tmp dir %s fail",dir.c_str());
    return NULL;
  }

  if (!fs->CreateDirectory(tmpdir)) {
    Errorf("create wal tmp dir %s fail",dir.c_str());
    return NULL;
  }

  string file = fs->Join(tmpdir, walName(0,0));

  return NULL;
}

WAL* 
OpenWAL(walOption* option, const string& dir, const WalSnapshot& snapshot) { 
  WAL *wal = openAtIndex(option, dir, snapshot, true);
  if (!wal) {
    return NULL;
  }

  return wal;
}

WAL* 
OpenForRead(walOption* option, const string& dir, const WalSnapshot& snapshot) { 
  return openAtIndex(option, dir, snapshot, false);
}

static WAL* 
openAtIndex(walOption* option, const string& dir, const WalSnapshot& snapshot, bool write) {
  vector<string> names;

  if (!readWalNames(option->fs, dir, &names)) {
    return NULL;
  }

  int nameIndex = searchIndex(names, snapshot.index());
  if (nameIndex < 0 || !isValidSeq(names, nameIndex)) {
    return NULL;
  }

  // open the wal files
  return NULL;
}

}; // namespace libraft