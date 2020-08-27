/*
 * Copyright (C) lichuang
 */

#pragma once

#include <stdarg.h>
#include <string>

using namespace std;

namespace libraft {

// Return a C++ string given printf-like input.
extern string StringPrintf(const char *fmt, ...);

// Append result to a supplied string.
extern void StringAppendf(string*, const char *fmt, ...);

extern uint64_t HashString(const string& str);
extern string StringToHex(const string& str);
extern bool PopenToString(const char *command, string *result);

};
