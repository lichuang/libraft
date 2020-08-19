/*
 * Copyright (C) lichuang
 */

#pragma once

#include "base/define.h"
#include "base/error.h"
#include "base/typedef.h"

namespace libraft {

class MessageHandler;

class IMessage {
  friend class MessageHandler;
public:
  virtual ~IMessage() {}

  MessageType Type() const { return type_; }
  
protected:
  MessageId id_;
  EntityId srcId_;
  EntityId dstId_;
  Error error_;
  MessageType type_;
  bool isResponse_;
};

class MessageHandler {
public:

  virtual ~MessageHandler() {}
};

};