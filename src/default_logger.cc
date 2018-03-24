#include "default_logger.h"

void DefaultLogger::log(const char *level, const char *file, int line, const char *fmt, va_list args) {
  int n;

  n = snprintf(buf_, kMaxLogBufSize, "[%s %s:%d]", level, file, line);
  n += vsnprintf(buf_ + n, kMaxLogBufSize - n, fmt, args);
  buf_[n++] += '\n';
  buf_[n++] += '\0';

  fprintf(stdout, "%s", buf_);
  fflush(stdout);
}

DefaultLogger kDefaultLogger;
