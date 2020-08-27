/*
 * Copyright (C) codedump
 */

#include <stdio.h>
#include <iostream>
#include <sstream>
#include <vector>
#include "util/hash.h"
#include "util/string.h"

namespace libraft {

// small fixed size for stringAppendV
const static int kSmallFixedSize = 1024;

// max append string size
const static int kMaxAppendSize = 32 * 1024 * 1024;

void 
stringAppendV(string* dst, const char* format, va_list ap) {
  // First try with a small fixed size buffer
  char space[kSmallFixedSize];

  // It's possible for methods that use a va_list to invalidate
  // the data in it upon use.  The fix is to make a copy
  // of the structure before using it and use that copy instead.
  va_list ap_copy;
  va_copy(ap_copy, ap);
  int result = vsnprintf(space, sizeof(space), format, ap_copy);
  va_end(ap_copy);

  if ((result >= 0) && (result < static_cast<int>(sizeof(space)))) {
    // It fit
    dst->append(space, result);
    return;
  }

  // Repeatedly increase buffer size until it fits
  int length = sizeof(space);
  while (true) {
    if (result < 0) {
      // Older behavior: just try doubling the buffer size
      length *= 2;
    } else {
      // We need exactly "result+1" characters
      length = result+1;
    }

    if (length > kMaxAppendSize) {
      // That should be plenty, don't try anything larger.
      return;
    }
    std::vector<char> mem_buf(length);

    // Restore the va_list before we use it again
    va_copy(ap_copy, ap);
    result = vsnprintf(&mem_buf[0], length, format, ap_copy);
    va_end(ap_copy);

    if ((result >= 0) && (result < length)) {
      // It fit
      dst->append(&mem_buf[0], result);
      return;
    }
  }
}

string 
StringPrintf(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  string result;
  stringAppendV(&result, fmt, ap);
  va_end(ap);
  return result;
}

void 
StringAppendf(string* ret, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  stringAppendV(ret, fmt, ap);
  va_end(ap);
}

uint64_t 
HashString(const string& str) {
  return HashBytes(str.c_str(), static_cast<int>(str.size()));
}

string 
StringToHex(const string& str) {
  string hex = "0x";
  for (size_t i = 0; i < str.size(); ++i) {
    hex += StringPrintf("%x", static_cast<uint8_t>(str[i]));
  }
  return hex;
}

bool 
PopenToString(const char *command, string *result) {
  result->clear();
  FILE *fp = popen(command, "r");
  if (!fp) {
    return false;
  }
  char buffer[200];
  while (fgets(buffer, sizeof(buffer), fp)) {
    *result += buffer;
  }
  pclose(fp);
  return true;
}

};