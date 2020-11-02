/*
 * Copyright (C) lichuang
 */

#pragma once

#include <string>
#include <list>
#include "<rocksdb/db.h>"
#include "<rocksdb/slice.h>"
#include "<rocksdb/options.h>"

using namespace std;
using namespace ROCKSDB_NAMESPACE;

namespace libraft {

class ConfigurationManager;

class RocksDbLogStorage {
public:
  RocksDbLogStorage(const string& path, bool sync);

  virtual ~RocksDbLogStorage() {}

  // init logstorage, check consistency and integrity
  virtual bool init(ConfigurationManager* conf_manager) = 0;

  // first log index in log
  virtual int64_t firstLogIndex() = 0;

  // last log index in log
  virtual int64_t lastLogIndex() = 0;

  // get logentry by index
  virtual LogEntry* getEntry(const int64_t index) = 0;

  // get logentry's term by index
  virtual int64_t getTerm(const int64_t index) = 0;

  // append entries to log
  virtual int appendEntry(const LogEntry* entry) = 0;

  // append entries to log, return append success number
  virtual int appendEntries(const std::vector<LogEntry*>& entries) = 0;

  // delete logs from storage's head, [first_log_index, first_index_kept) will be discarded
  virtual int truncatePrefix(const int64_t first_index_kept) = 0;

  // delete uncommitted logs from storage's tail, (last_index_kept, last_log_index] will be discarded
  virtual int truncateSuffix(const int64_t last_index_kept) = 0;

  // Drop all the existing logs and reset next log index to |next_log_index|.
  // This function is called after installing snapshot from leader
  virtual int reset(const int64_t next_log_index) = 0;

private:
  bool initAndLoad(ConfigurationManager* conf_manager);
  bool loadConfig();
private:
  string  path_;
  bool    sync_;
  DB*     db_;
  Options dbOptions_;
  WriteOptions writeOptions_;
  ReadOptions  readOptions_;
  list<ColumnFamilyOptions> cfOptions_;
  ColumnFamilyHandle* dataHandle_;
  ColumnFamilyHandle* confHandle_;
  uint64_t firstLogIndex_;
  bool hasLoadFirstLogIndex_;
};
};