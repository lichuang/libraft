/*
 * Copyright (C) lichuang
 */

#include "libraft.h"
#include "base/logger.h"
#include <libgen.h> // for basename
#include <stdio.h>

namespace libraft {

log_level_e gLogLevel = Debug;

static void  default_logger(const char * buf);
const static int kLogBufferSize = 1024;
static raft_log_func gLogFunc = default_logger;

const static char* kLogString[] = {
  "D",
  "W",
  "I",
  "E",
  "F",
};

static void
default_logger(const char * buf) {
  fprintf(stdout, "%s", buf);
  fflush(stdout);
}

void 
do_log(log_level_e level, const char *file, int line, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  int n;
  char buf[kLogBufferSize] = {'\0'};

  n =  snprintf(buf, kLogBufferSize, "[%s:%d %s]", basename((char*)file), line, kLogString[level]);
  n += vsnprintf(buf + n, kLogBufferSize - n, fmt, args);
  va_end(args);

  if (n + 2 > kLogBufferSize) {
    return;
  }

  buf[n++] += '\n';
  buf[n++] += '\0';

  gLogFunc(buf);
}

}; // namespace libraft
