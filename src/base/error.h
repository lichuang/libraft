/*
 * Copyright (C) codedump
 */

#ifndef __LIBRAFT_BASE_ERROR_H__
#define __LIBRAFT_BASE_ERROR_H__

#include <string>

using namespace std;

BEGIN_NAMESPACE

enum {
  EOK = 0;
};

class Error {
public:
  Error(int code=EOK, const std::string& msg="")
    : errCode_(code), errMsg_(msg) {
  }

  const int Code() const { return errCode_; }

  const std::string& Message() const { return errMsg_; }

private:
  int errCode_;
  string msg_;
};

END_NAMESPACE

#endif // __LIBRAFT_BASE_ERROR_H__
