/*
 * Copyright (C) lichuang
 */

#ifndef __LIBRAFT_LOGGER_H__
#define __LIBRAFT_LOGGER_H__

#include <stdarg.h>
#include <stdlib.h>
#include "libraft.h"

namespace libraft {

void do_log(LogLevel level, const char *file, int line, const char *fmt, ...);

extern LogLevel gLogLevel;

#define Debugf(fmt, ...) if (gLogLevel <= Debug) do_log(Debug, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Warnf(fmt, ...)  if (gLogLevel <= Warn)  do_log(Warn, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Infof(fmt, ...)  if (gLogLevel <= Info)  do_log(Info,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Errorf(fmt, ...) if (gLogLevel <= Error) do_log(Error, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Fatalf(fmt, ...) do_log(Fatal, __FILE__, __LINE__, fmt, ##__VA_ARGS__);abort()

extern void initLog();

}; // namespace libraft

#endif  // __LIBRAFT_LOGGER_H__
