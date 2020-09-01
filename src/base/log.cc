/*
 * Copyright (C) lichuang
 */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <libgen.h>
#include <gflags/gflags.h>
#include <sstream>
#include <string>
#include "base/worker.h"
#include "base/log.h"
#include "base/logger.h"

using namespace std;

namespace libraft {

DEFINE_int32(loglevel, 0, "min log level"
             "0=DEBUG 1=INFO 2=WARNING 3=ERROR 4=FATAL");

const char* kLogLevelName[NUM_LOG_LEVELS] = {
  "[D ",
  "[I ",
  "[W ",
  "[E ",
  "[F ",
};

void 
defaultOutput(const char* msg, int len) {
  fwrite(msg, 1, len, stdout);
}

void defaultFlush() {
  fflush(stdout);
}

LogMessage::OutputFunc gOutputput = defaultOutput;
LogMessage::FlushFunc gFlush = defaultFlush;

LogMessage::LogMessage(const char* file, int line, int32_t level, const char* func)
  : data_(new LogMessageData()),
    level_(level) {
  Stream() << kLogLevelName[level] << CurrentThreadName() 
    << " " << CurrentLogtimeString() 
    << " " << basename(const_cast<char*>(file)) 
    << ':' << line << ']';
}

LogMessage::~LogMessage() {
  finish();
  
  SendLog(data_);
  if (level_ == FATAL) {
    string stack;
    //GetCallStack(&stack);
    Stream() << stack;
    Flush(true);
    // TODO: wait for log thread exit
    abort();
  }
}

void
LogMessage::finish() {
  Stream() << "\n";
  data_->text_[data_->stream_.pcount()] = '\0';
}

int 
logLevel() {
  return FLAGS_loglevel;
}

void 
LogMessage::setOutput(OutputFunc out) {
  gOutputput = out;
}

void 
LogMessage::setFlush(FlushFunc flush) {
  gFlush = flush;
}  
};