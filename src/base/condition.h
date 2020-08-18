/*
 * Copyright (C) lichuang
 */

#pragma once

#include <pthread.h>
#include "base/define.h"
#include "base/mutex.h"

namespace libraft {

class Condition {
public:
  Condition();

  ~Condition();

  void Wait(Mutex *mutex);

  void Notify();
  void NotifyAll();

private:
  pthread_cond_t cond_;

  DISALLOW_COPY_AND_ASSIGN(Condition);
};

};