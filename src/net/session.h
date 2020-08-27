/*
 * Copyright (C) lichuang
 */

#pragma once

#include "base/typedef.h"

namespace libraft {

class Session {
public:
  Session();

private:

};

class SessionFactory {
public:
  SessionFactory(){}
  virtual ~SessionFactory() {
  }

  virtual Session* NewSession(fd_t fd) = 0;
};
};