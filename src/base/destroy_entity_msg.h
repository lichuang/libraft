/*
 * Copyright (C) lichuang
 */

#pragma once

#include "base/message.h"
#include "base/message_type.h"

namespace libraft {

class IEntity;

struct destroyEntityMsg : public IMessage {
public:
  destroyEntityMsg(IEntity* en)
    : IMessage(kDestroyEntityMessage),
      entity_(en) {
  }

  IEntity* entity_;
};  
};