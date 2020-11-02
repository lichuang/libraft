/*
 * Copyright (C) lichuang
 */

#include <vector>
#include "base/log.h"
#include "raft/storage/rocksdb_log_storage.h"

namespace libraft {

static const string kConfigFamilyName = "RaftConfiguration";
static const string kDataFamilyName   = "RaftData";

RocksDbLogStorage::RocksDbLogStorage(const string& path, bool sync)
  : path_(path),
    sync_(sync),
    db_(nullptr),
    dataHandle_(nullptr),
    confHandle_(nullptr) {

}

RocksDbLogStorage::~RocksDbLogStorage() {}

bool 
RocksDbLogStorage::init(ConfigurationManager* conf_manager) {
  if (db_) {
    Warn() << "RocksDbLogStorage has been init";
    return true;
  }

  // init options
  dbOptions_.create_if_missing = true;

  writeOptions_.sync = sync_;
  readOptions_.total_order_seek = true;
}

bool 
RocksDbLogStorage::initAndLoad(ConfigurationManager* conf_manager) {  
  hasLoadFirstLogIndex_ = false;
  firstLogIndex_ = 1;
  
  // create config and data column family
  ColumnFamilyOptions cfOption;
  vector<string> cfNames;
  vector<ColumnFamilyHandle*> cfHandles;
  cfNames.push_back(kConfigFamilyName);
  cfNames.push_back(kDataFamilyName);

  rocksdb::Status status = db_->CreateColumnFamilies(cfOption, cfNames, &cfHandles);
  if (!status.ok()) {
    Error() << "CreateColumnFamilies fail:" << status.Code();
    return false;
  }

  confHandle_ = cfHandles[0];
  dataHandle_ = cfHandles[1];

  return true;
}

};