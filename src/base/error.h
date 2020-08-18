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

class Error {
public:
  Error(int code=kOK, const std::string& msg="")
    : errCode_(code), errMsg_(msg) {
  }

  int Code() const { return errCode_; }

  const std::string& Message() const { return errMsg_; }

private:
  int errCode_;
  string errMsg_;
};

};