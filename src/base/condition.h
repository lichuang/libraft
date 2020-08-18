/*
 * Copyright (C) lichuang
 */

#pragma once

#include <pthread.h>
#include "base/define.h"
#include "base/mutex.h"

BEGIN_NAMESPACE

class Condition {
public:
  Condition();

  ~Condition();

  void Wait(Mutex *mutex);

  // return true if notified, else(include timeout) false
  bool WaitUntil(Mutex *, int timeout_ms);

  void Notify();
  void NotifyAll();

private:
  pthread_cond_t cond_;

  DISALLOW_COPY_AND_ASSIGN(Condition);
};

END_NAMESPACE