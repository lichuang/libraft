/*
 * Copyright (C) lichuang
 */

#pragma once

#include <stdint.h>

namespace libraft {

typedef uint64_t MessageId;

typedef uint32_t MessageType;

typedef uint64_t EntityId;
typedef uint32_t EntityType;

typedef uint32_t TimerEventId;

typedef int fd_t;

typedef uint64_t MethodId;

// invalid fd const
static const fd_t kInvalidFd     = -1;

};
