/*
 * Copyright (C) lichuang
 */

#pragma once

#include "base/entity.h"
#include "base/singleton.h"

namespace libraft {

struct LogMessageData;
class Worker;
class IEntity;

class Logger {
  friend class Singleton<Logger>;

public:
  ~Logger();

  void Send(LogMessageData *);
  void Flush(bool end);

private:
  Logger();
  void doInit();

private:
  Worker *worker_;
  IEntity *logger_entity_;
};

#define gLogger libraft::Singleton<libraft::Logger>::Instance()

// send log message data to logger 
extern void SendLog(LogMessageData *data);

// flush log data into file
extern void Flush(bool end);

// return current log time string
extern const char* CurrentLogtimeString();
};