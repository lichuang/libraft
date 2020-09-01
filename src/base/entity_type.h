/*
 * Copyright (C) lichuang
 */

#pragma once

#include "base/typedef.h"

// every entity type MUST BE defined here
namespace libraft {
static const EntityType kWorkerEntity         = 1;
static const EntityType kLoggerEntity         = 2;
static const EntityType kSessionEntity        = 3;
}