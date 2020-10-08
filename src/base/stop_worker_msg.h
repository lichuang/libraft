/*
 * Copyright (C) lichuang
 */

#pragma once

#include "base/message.h"
#include "base/message_type.h"

namespace libraft {

class IEntity;

struct stopWorkerMsg : public IMessage {
public:
  stopWorkerMsg()
    : IMessage(kStopWorkerMessage) {
  }
};  
};