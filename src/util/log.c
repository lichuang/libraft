/*
 * Copyright (C) lichuang
 */

#include <libgen.h>
#include <stdio.h>
#include "util/log.h"

log_level_e g_log_level = Debug;

static void  default_logger(log_level_e level, const char * buf);
const static int k_log_buffer_length = 1024;
const static int g_default_logger = 1024;

const static char* g_log_string[] = {
  "D",
  "I",
  "E",
  "F",
};

static void
default_logger(log_level_e level, const char * buf) {
  fprintf(stdout, "%s", buf);
  fflush(stdout);
}

void 
do_log(log_level_e level, const char *file, int line, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  int n;
  char buf[k_log_buffer_length] = {'\0'};

  n =  snprintf(buf, k_log_buffer_length, "[%s:%d %s]", basename((char*)file), line, g_log_string[level]);
  n += vsnprintf(buf + n, k_log_buffer_length - n, fmt, args);
  va_end(args);

  if (n + 2 > k_log_buffer_length) {
    return;
  }

  buf[n++] += '\n';
  buf[n++] += '\0';

  default_logger(level, buf);
}