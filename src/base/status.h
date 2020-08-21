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
    : errCode_(code), errMsg_(msg) {
  }

  int Code() const { return errCode_; }

  const std::string& Message() const { return errMsg_; }

  Status& operator= (const Status& error) {
    errCode_ = error.errCode_;
    errMsg_ = error.errMsg_;

    return *this;
  }

private:
  int errCode_;
  string errMsg_;
};

};