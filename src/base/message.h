/*
 * Copyright (C) lichuang
 */

#pragma once

#include "base/define.h"
#include "base/entity.h"
#include "base/error.h"
#include "base/message_type.h"
#include "base/typedef.h"

namespace libraft {

class MessageHandler;

struct IMessage {
public:
  IMessage(MessageType typ, bool isResponse = false) 
    : type_(typ),
      isResponse_(isResponse) {}

  virtual ~IMessage() {}
  
  void setDstEntiity(const EntityRef& ref) {
    dstRef_ = ref;
  }

  void setSrcEntiity(const EntityRef& ref) {
    srcRef_ = ref;
  }

  void responseFor(IMessage *msg) {
    isResponse_ = true;
    srcRef_ = msg->dstRef_;
    dstRef_ = msg->srcRef_;
  }

  MessageId id_;
  EntityRef srcRef_;
  EntityRef dstRef_;
  Error error_;
  MessageType type_;
  bool isResponse_;
};

class MessageHandler {
public:

  virtual ~MessageHandler() {}
};

};