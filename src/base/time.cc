/*
 * Copyright (C) lichuang
 */

#include "base/time.h"

namespace libraft {

void
Localtime(const time_t& unix_sec, struct tm* result) {
    static const int kHoursInDay = 24;
    static const int kMinutesInHour = 60;
    static const int kDaysFromUnixTime = 2472632;
    static const int kDaysFromYear = 153;
    static const int kMagicUnkonwnFirst = 146097;
    static const int kMagicUnkonwnSec = 1461;

    result->tm_sec = static_cast<int>(unix_sec % kMinutesInHour);
    int i      = static_cast<int>(unix_sec/kMinutesInHour);
    result->tm_min  = i % kMinutesInHour; //nn
    i /= kMinutesInHour;
    result->tm_hour = static_cast<int>((i + 8) % kHoursInDay); // hh
    result->tm_mday = static_cast<int>((i + 8) / kHoursInDay);
    int a = result->tm_mday + kDaysFromUnixTime;
    int b = (a*4  + 3)/kMagicUnkonwnFirst;
    int c = (-b*kMagicUnkonwnFirst)/4 + a;
    int d =((c*4 + 3) / kMagicUnkonwnSec);
    int e = -d * kMagicUnkonwnSec;
    e = e/4 + c;
    int m = (5*e + 2)/kDaysFromYear;
    result->tm_mday = -(kDaysFromYear * m + 2)/5 + e + 1;
    result->tm_mon = (-m/10)*12 + m + 2;
    result->tm_year = b*100 + d  - 6700 + (m/10);
}

};