/*
 * Copyright (C) lichuang
 */

#pragma once

#include <iostream>
#include "base/likely.h"
#include "base/define.h"

namespace libraft {

const int DEBUG = 0;
const int INFO  = 1;
const int WARN  = 2;
const int ERROR = 3;
const int FATAL = 4;
const int NUM_LOG_LEVELS = FATAL + 1;

// An arbitrary limit on the length of a single log message.  This
// is so that streaming can be done more efficiently.
static const size_t kMaxLogMessageLen = 30000;

class LogStreamBuf : public std::streambuf {
public:
  // REQUIREMENTS: "len" must be >= 2 to account for the '\n' and '\0'.
  LogStreamBuf(char *buf, int len) {
    setp(buf, buf + len - 2);
  }

  // This effectively ignores overflow.
  virtual int_type overflow(int_type ch) {
    return ch;
  }

  // Legacy public ostrstream method.
  size_t pcount() const { return pptr() - pbase(); }
  char* pbase() const { return std::streambuf::pbase(); }
};

// for log stream
class LogStream : public std::ostream {
public:
  LogStream(char *buf, int len, int ctr)
    : std::ostream(NULL),
      streambuf_(buf, len),
      ctr_(ctr),
      self_(this) {
    rdbuf(&streambuf_);
  }

  int ctr() const { return ctr_; }
  void set_ctr(int ctr) { ctr_ = ctr; }
  LogStream* self() const { return self_; }

  // Legacy std::streambuf methods.
  size_t pcount() const { return streambuf_.pcount(); }
  char* pbase() const { return streambuf_.pbase(); }
  char* str() const { return pbase(); }

private:
  DISALLOW_COPY_AND_ASSIGN(LogStream);

  LogStreamBuf streambuf_;
  int ctr_;  // Counter hack (for the LOG_EVERY_X() macro)
  LogStream *self_;  // Consistency check hack
};
    
// where log data contents in
struct LogMessageData {
public:  
  char text_[kMaxLogMessageLen+1];
  LogStream stream_;

  LogMessageData()
    : stream_(text_, kMaxLogMessageLen, 0) {
  }
};

class LogMessage {
public:
  LogMessage(const char* file, int line, int level, const char* func);
  ~LogMessage();

  std::ostream& Stream() {
    return data_->stream_;
  }

  static int logLevel();
  static void setLogLevel(int level);

  typedef void (*OutputFunc)(const char* msg, int len);
  typedef void (*FlushFunc)();
  static void setOutput(OutputFunc);
  static void setFlush(FlushFunc);

private:
  void init();
  void finish();

private:
  static int logLevel_;
  LogMessageData* data_;
  int level_;
};

#define Debug if (libraft::LogMessage::logLevel() <= libraft::DEBUG) \
  libraft::LogMessage(__FILE__, __LINE__, libraft::DEBUG, __func__).Stream

#define Info if (libraft::LogMessage::logLevel() <= libraft::INFO) \
  libraft::LogMessage(__FILE__, __LINE__, libraft::INFO, __func__).Stream

#define Warn if (libraft::LogMessage::logLevel() <= libraft::WARN) \
  libraft::LogMessage(__FILE__, __LINE__, libraft::WARN, __func__).Stream

#define Error if (libraft::LogMessage::logLevel() <= libraft::ERROR) \
  libraft::LogMessage(__FILE__, __LINE__, libraft::ERROR, __func__).Stream

#define Fatal if (libraft::LogMessage::logLevel() <= libraft::FATAL) \
  libraft::LogMessage(__FILE__, __LINE__, libraft::FATAL, __func__).Stream

// This class is used to explicitly ignore values in the conditional
// logging macros.  This avoids compiler warnings like "value computed
// is not used" and "statement has no effect".
class LogMessageVoidify {
 public:
  LogMessageVoidify() { }

	void operator&(std::ostream&) { }
};

#define FATAL_IF(condition) \
  !(condition) ? (void) 0 : LogMessageVoidify() \
  & libraft::LogMessage(__FILE__, __LINE__, libraft::FATAL, __func__).Stream()

#define ERROR_IF(condition) \
  !(condition) ? (void) 0 : LogMessageVoidify() \
  & libraft::LogMessage(__FILE__, __LINE__, libraft::ERROR, __func__).Stream()

#define ASSERT(condition) \
	FATAL_IF(unlikely(!(condition))) << "Assertion failed: " #condition << " "

#define ASSERT_EQUAL(c1, c2) \
	FATAL_IF(unlikely((c1) != (c2))) << "Assertion failed: " #c1 << " not equal to " #c2 << " "

#define ASSERT_NOT_EQ(c1, c2) \
	FATAL_IF(unlikely((c1) == (c2))) << "Assertion failed: " #c1 << " equal to " #c2 << " "

};