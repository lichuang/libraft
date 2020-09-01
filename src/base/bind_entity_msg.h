/*
 * Copyright (C) lichuang
 */

#pragma once

#include "base/message.h"

namespace libraft {

class IEntity;

struct bindEntityMsg: public IMessage {
public:
  bindEntityMsg(IEntity* en)
    : IMessage(kBindEntityMessage),
      entity_(en) {
  }

  IEntity* entity_;
};
}