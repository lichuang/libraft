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
  IMessage() : next_(NULL) {}
  virtual ~IMessage() {}

  MessageType Type() const { return type_; }
  
  void Next(IMessage* next) {
    next_ = next;
  }

  IMessage* Next() {
    return next_;
  }

protected:
  MessageId id_;
  EntityId srcId_;
  EntityId dstId_;
  Error error_;
  MessageType type_;
  bool isResponse_;
  IMessage *next_;
};

class MessageHandler {
public:

  virtual ~MessageHandler() {}
};

};