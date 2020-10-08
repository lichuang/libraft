/*
 * Copyright (C) lichuang
 */

#pragma once

// every message type MUST BE defined here
namespace libraft {
static const int kLogMessage            = 1;
static const int kAddTimerMessage       = 2;
static const int kBindEntityMessage     = 3;
static const int kDestroyEntityMessage  = 4;
static const int kStopWorkerMessage     = 5;

// rpc related message
static const int kRpcCallMethodMessage      = 20;

}