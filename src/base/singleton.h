/*
 * Copyright (C) lichuang
 */

#pragma once
#include <pthread.h>

namespace libraft {

template <typename T>
class Singleton {
public:
  static T* Instance() {
    ::pthread_once(&ponce_, &Singleton::init);
    return value_;
  }

private:  
  static void init() {
    value_ = new T();
    value_->doInit();
  }

private:
  DISALLOW_COPY_AND_ASSIGN(Singleton<T>);
  static pthread_once_t ponce_;
  static T* value_;
};

template<typename T>
pthread_once_t Singleton<T>::ponce_ = PTHREAD_ONCE_INIT;

template<typename T>
T* Singleton<T>::value_ = NULL;
};