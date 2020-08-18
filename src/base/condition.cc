/*
 * Copyright (C) lichuang
 */

#include "base/condition.h"

namespace libraft {

Condition::Condition() {
  pthread_cond_init(&cond_, NULL);      
}

Condition::~Condition() {
  ::pthread_cond_destroy(&cond_);
}

void
Condition::Wait(Mutex *mutex) {
  ::pthread_cond_wait(&cond_, &(mutex->mutex_));
}

void
Condition::Notify() {
  ::pthread_cond_signal(&cond_);
}

void
Condition::NotifyAll() {
  ::pthread_cond_broadcast(&cond_);
}

};