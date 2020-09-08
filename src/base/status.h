/*
 * Copyright (C) lichuang
 */

#pragma once

#include <errno.h>
#include <string>

using namespace std;

namespace libraft {

enum {
  kOK = 0,
  kError = -1,
  kTryIOAgain = 1,
};

// Status class describe operation status
class Status {
public:
  Status(int code=kOK, const std::string& msg="OK")
    : code_(code), msg_(msg) {
  }

  bool Ok() const { return code_ == kOK; }
  bool TryIOAgain() const { return code_ == kTryIOAgain; }

  int Code() const { return code_; }

  const std::string& String() const { return msg_; }

  Status& operator= (const Status& error) {
    code_ = error.code_;
    msg_ = error.msg_;

    return *this;
  }

  Status& operator= (int code) {
    code_ = code;

    return *this;
  }

private:
  int code_;
  string msg_;
};



};