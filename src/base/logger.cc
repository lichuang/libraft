/*
 * Copyright (C) lichuang
 */

#include <time.h>
#include <sys/time.h>
#include <gflags/gflags.h>
#include "base/entity.h"
#include "base/entity_type.h"
#include "base/message.h"
#include "base/time.h"
#include "base/worker.h"
#include "base/log.h"
#include "base/logger.h"

namespace libraft {

DEFINE_string(logpath, "/tmp/", "log file dir");
DEFINE_string(app, "libraft_app", "app name");

DEFINE_int32(update_time_internal, 100, "update time string internal,in ms");

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
  loggerEntity(Worker* w, Logger* logger) : IEntity(kLoggerEntity), logger_(logger) {
    w->AddEntity(this);
  }

  virtual ~loggerEntity() {
  }

  void Handle(IMessage* m) {
    logMessage *msg = (logMessage*)m;
    logger_->processLog(msg->data);
    delete msg->data;
  }

  void onTimeout(ITimerEvent*) {
    logger_->updateTime();
  }  

  Logger* logger_;
};

Logger::Logger() {
  worker_ = new Worker("logger", kLogThread);

  // init time
  index_.store(0, std::memory_order_relaxed);
  updateTime();

  logger_entity_ = new loggerEntity(worker_, this);
  worker_->RunEvery(logger_entity_, Duration(FLAGS_update_time_internal));
}

Logger::~Logger() {
  worker_->Stop();
  delete worker_;
}

void
Logger::processLog(LogMessageData *data) {
  printf("%s\n", data->text_);
}

void
Logger::updateTime() {
  Time *tp;
  uint64_t *tmsec;
  char* logStr;
  time_t sec;
  uint64_t msec;
  int index;

  index_.store((index_ + 1) % kTimeSlots, std::memory_order_relaxed);

  index = index_.load(memory_order_acquire);
  tp = &cached_times_[index];
  tmsec = &cached_msecs_[index];
  logStr = &(cached_log_time_strs_[index][0]);

  struct timeval tv;
  ::gettimeofday(&tv, NULL);

  sec = tv.tv_sec;
  msec = tv.tv_usec / 1000;

  *tmsec = sec * 1000 + msec;
  tp->sec = sec;
  tp->msec = msec;

  struct tm tim;
	Localtime(tv.tv_sec, &tim);
  
  snprintf(logStr, kLogTimeStringLength,
    "%4d/%02d/%02d %02d:%02d:%02d.%03d",
    tim.tm_year + 1900, tim.tm_mon + 1, tim.tm_mday,
    tim.tm_hour, tim.tm_min, tim.tm_sec, static_cast<int>(msec)); 
}

void
Logger::Send(LogMessageData *data) {
  Sendto(logger_entity_->Ref(), new logMessage(data));
}

void
Logger::doInit() {

}

void
Logger::Flush(bool end) {
  //flush();	
}

void
SendLog(LogMessageData *data) {
  gLogger->Send(data);
}

void 
Flush(bool end) {
	gLogger->Flush(end);
}

};