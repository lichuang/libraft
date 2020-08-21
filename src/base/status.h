/*
 * Copyright (C) lichuang
 */

#pragma once

#include <string>

using namespace std;

namespace libraft {

enum {
  kOK = 0,
  kError = -1,
};

// Status class describe operation status
class Status {
public:
  Status(int code=kOK, const std::string& msg="")
    : code_(code), msg_(msg) {
  }

  int Code() const { return code_; }

  const std::string& Message() const { return msg_; }

  Status& operator= (const Status& error) {
    code_ = error.code_;
    msg_ = error.msg_;

    return *this;
  }

private:
  int code_;
  string msg_;
};

};