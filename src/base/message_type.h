/*
 * Copyright (C) lichuang
 */

#pragma once

// every message type MUST BE defined here
namespace libraft {
static const int kLogMessage          = 1;
static const int kAddTimerMessage     = 2;
static const int kBindEntityMessage   = 3;
}