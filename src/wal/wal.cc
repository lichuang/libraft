/*
 * Copyright (C) lichuang
 */

#include "base/file_system_adaptor.h"
#include "wal/util.h"
#include "wal/wal.h"

namespace libraft {

static WAL* openAtIndex(walOption*, const string&, const Snapshot&, bool);

WAL* 
createWAL(walOption* option, const string& dir,const string& meta) {
  FileSystemAdaptor* fs = option->fs;
  Logger* logger = option->logger;

  if (fs->PathExists(dir)) {
    logger->Errorf(__FILE__, __LINE__, "wal dir %s exists",dir.c_str());
    return NULL;
  }

  // keep temporary wal directory so WAL initialization appears atomic
  string tmpdir = dir + ".tmp";
  if (fs->DirectoryExists(tmpdir) && !fs->DeleteFile(tmpdir, true)) {
    logger->Errorf(__FILE__, __LINE__, "delete wal tmp dir %s fail",dir.c_str());
    return NULL;
  }

  if (!fs->CreateDirectory(tmpdir)) {
    logger->Errorf(__FILE__, __LINE__, "create wal tmp dir %s fail",dir.c_str());
    return NULL;
  }

  string file = fs->Join(tmpdir, walName(0,0));

}

WAL* 
OpenWAL(walOption* option, const string& dir, const Snapshot& snapshot) { 
  WAL *wal = openAtIndex(option, dir, snapshot, true);
  if (!wal) {
    return NULL;
  }

  return wal;
}

WAL* 
OpenForRead(walOption* option, const string& dir, const Snapshot& snapshot) { 
  return openAtIndex(option, dir, snapshot, false);
}

static WAL* 
openAtIndex(walOption* option, const string& dir, const Snapshot& snapshot, bool write) {
  vector<string> names;

  if (!readWalNames(option->fs, dir, &names)) {
    return NULL;
  }

  int nameIndex = searchIndex(names, snapshot.Index);
  if (nameIndex < 0 || !isValidSeq(names, nameIndex)) {
    return NULL;
  }

  // open the wal files

}

}; // namespace libraft