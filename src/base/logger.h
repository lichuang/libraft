/*
 * Copyright (C) lichuang
 */

#ifndef __LIBRAFT_UTIL_LOGGER_H__
#define __LIBRAFT_UTIL_LOGGER_H__

#include <stdarg.h>
#include <stdlib.h>

namespace libraft {

typedef enum log_level_e {
  Debug     = 0,
  Warn      = 1,
  Info      = 2,
  Error     = 3,
  Fatal     = 4,

  // disable all libraft log
  NoneLog,
} log_level_e;

void do_log(log_level_e level, const char *file, int line, const char *fmt, ...);

extern log_level_e gLogLevel;

#define Debugf(fmt, ...) if (gLogLevel <= Debug) do_log(Debug, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Warnf(fmt, ...)  if (gLogLevel <= Warn)  do_log(Warn, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Infof(fmt, ...)  if (gLogLevel <= Info)  do_log(Info,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Errorf(fmt, ...) if (gLogLevel <= Error) do_log(Error, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Fatalf(fmt, ...) do_log(Fatal, __FILE__, __LINE__, fmt, ##__VA_ARGS__);abort()

extern void initLog();

}; // namespace libraft

#endif  // __LIBRAFT_UTIL_LOGGER_H__
