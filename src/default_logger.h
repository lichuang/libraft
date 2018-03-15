#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <stdio.h>
#include "libraft.h"

const static int kMaxLogBufSize = 2048;

class DefaultLogger : public Logger{
private:
  void log(const char *level, const char *file, int line, const char *fmt, ...) {
    va_list args;
    int     n;

    va_start(args, fmt);
    va_end(args);

    char buf[kMaxLogBufSize] = {0};
    n = snprintf(buf, kMaxLogBufSize, "[%s %s:%d]", level, file, line);
    n += vsnprintf(buf + n, kMaxLogBufSize - n, fmt, args);
    buf[n++] += '\n';
    buf[n++] += '\0';

    printf("%s", buf);
  }

public:
  void Debugf(const char *file, int line, const char *fmt, ...) {
    log("D", file, line, fmt);
  }
  void Infof(const char *file, int line, const char *fmt, ...) {
    log("I", file, line, fmt);
  }
  void Warningf(const char *file, int line, const char *fmt, ...) {
    log("W", file, line, fmt);
  }

  void Errorf(const char *file, int line, const char *fmt, ...) {
    log("E", file, line, fmt);
  }
  void Fatalf(const char *file, int line, const char *fmt, ...) {
    log("F", file, line, fmt);
  }
};

extern DefaultLogger kDefaultLogger;
#endif // __LOGGER_H__
