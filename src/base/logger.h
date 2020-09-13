/*
 * Copyright (C) lichuang
 */

#pragma once

#include <atomic>
#include <time.h>
#include "base/entity.h"

namespace libraft {

struct LogMessageData;
class Worker;
class IEntity;
class loggerEntity;
class Server;

// log time string length
static const int kLogTimeStringLength = sizeof("2019/01/01 00:00:00.000");

class Logger {
  friend class loggerEntity;
  friend class Server;

public:
  ~Logger();

  void Send(LogMessageData *);
  void Flush(bool end);

  // now time log string
  inline const char* GetNowLogtimeString() const {
    return const_cast<const char*>(cached_log_time_strs_[index_.load(memory_order_acquire)]);
  }

private:
  Logger();

  void updateTime();

  void processLog(LogMessageData *);

private:
  Worker *worker_;
  IEntity *logger_entity_;

  static const int kTimeSlots = 64;

  struct Time {
    time_t    sec;    // seconds since 1970.01.01 00:00:00
    uint64_t  msec;   // 
    int       gmtoff; // 
  };
  Time cached_times_[kTimeSlots];
  char cached_log_time_strs_[kTimeSlots][kLogTimeStringLength];
  uint64_t cached_msecs_[kTimeSlots];

  // current slots index
  std::atomic<int> index_;    
};

// send log message data to logger 
extern void SendLog(LogMessageData *data);

// flush log data into file
extern void Flush(bool end);

// return current log time string
extern const char* CurrentLogtimeString();

};