/*
 * Copyright (C) lichuang
 */

#ifndef __LIB_RAFT_LOGGER_H__
#define __LIB_RAFT_LOGGER_H__

#include <stdarg.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum log_level_e {
  Debug     = 0,
  Info      = 1,
  Error     = 2,
  Fatal     = 3,

  // disable all libraft log
  NoneLog,
} log_level_e;

void do_log(log_level_e level, const char *file, int line, const char *fmt, ...);

extern log_level_e g_log_level;

#define Debugf(fmt, ...) if (g_log_level <= Debug) do_log(Debug, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Infof(fmt, ...)  if (g_log_level <= Info)  do_log(Info,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Errorf(fmt, ...) if (g_log_level <= Error) do_log(Error, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Fatalf(fmt, ...) do_log(Fatal, __FILE__, __LINE__, fmt, ##__VA_ARGS__);abort()

#ifdef __cplusplus
}
#endif

#endif  // __LIB_RAFT_LOGGER_H__