#include "base/default_logger.h"

namespace libraft {
void DefaultLogger::log(const char *level, const char *file, int line, const char *fmt, va_list args) {
  int n;
  char buf[kMaxLogBufSize] = {'\0'};

  n = snprintf(buf, kMaxLogBufSize, "[%s %s:%d]", level, file, line);
  n += vsnprintf(buf + n, kMaxLogBufSize - n, fmt, args);
  buf[n++] += '\n';
  buf[n++] += '\0';

  fprintf(stdout, "%s", buf);
  fflush(stdout);
}

DefaultLogger kDefaultLogger;
}; // namespace libraft