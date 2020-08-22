/*
 * Copyright (C) lichuang
 */

#include <gflags/gflags.h>
#include "base/entity.h"
#include "base/message.h"
#include "base/worker.h"
#include "core/log.h"
#include "core/logger.h"

namespace libraft {

DEFINE_string(logpath, "/tmp/", "log file dir");
DEFINE_string(app, "libraft_app", "app name");

struct logMessage: public IMessage {
public:
  logMessage(LogMessageData *d)
    : IMessage(kLogMessage),
      data(d) {
  }

  LogMessageData *data;
};

class loggerEntity : public IEntity {
public:
  loggerEntity(Worker* w) : IEntity(w) {
  }

  virtual ~loggerEntity() {
  }

  void Handle(IMessage* m) {
    //logMessage *msg = (logMessage*)m;
  }

};

Logger::Logger() {
  worker_ = new Worker("logger");
  logger_entity_ = new loggerEntity(worker_);
}

Logger::~Logger() {
  worker_->Stop();
  delete worker_;
}

void
Logger::Send(LogMessageData *data) {
  Sendto(logger_entity_->Ref(), nullptr);
}

void
Logger::doInit() {

}

void
SendLog(LogMessageData *data) {
  Singleton<Logger>::Instance()->Send(data);
}

void 
Flush(bool end) {
	Singleton<Logger>::Instance()->Flush(end);
}

const char*
CurrentLogtimeString() {
  return "";
}
};