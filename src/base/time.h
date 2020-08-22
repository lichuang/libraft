/*
 * Copyright (C) lichuang
 */

#pragma once

#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <string>

using namespace std;

namespace libraft {

extern void Localtime(const time_t& unix_sec, struct tm* result);
};