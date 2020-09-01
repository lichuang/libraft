/*
 * Copyright (C) lichuang
 */

#pragma once

#include <list>
#include <pthread.h>
#include "base/define.h"
#include "base/singleton.h"
#include "base/log.h"

using namespace std;

namespace libraft {

// number per ObjectList alloc
static const int kAllocObjectNumber = 100;

template <typename T>
class ObjectList {
public:
  ObjectList() {
    allocate();
  }

  ~ObjectList() {
    typename list<T*>::iterator iter = free_list_.begin();

    while (iter != free_list_.end()) {
      T *obj = *iter;
      ++iter;
      delete obj;
    }
  }

  T* Get() {
    if (free_list_.empty()) {
      allocate();
    }
    T* obj = free_list_.front();
    free_list_.pop_front();
    ASSERT(obj != NULL);
    return obj;
  }

  void Free(T *obj) {
    ASSERT(obj != NULL);
    obj->Reset();
    free_list_.push_back(obj);
  }

private:
  void allocate() {
    int i;

    for (i = 0; i < kAllocObjectNumber; ++i) {
      T* obj = new T();
      if (obj != NULL) {
        free_list_.push_back(obj);
      }
    }
  }

private:
  DISALLOW_COPY_AND_ASSIGN(ObjectList);
  list<T*> free_list_;
};

template <typename T>
class ObjectPool {
  friend class Singleton<ObjectPool<T> >;

public:
  T* Get() {
    return obj_list_.Get();
  }
  void Free(T *obj) {
    obj_list_.Free(obj);
  }

private:
  ObjectPool() {
  }

  void doInit() {
    // nothing to do
  }

private:
  thread_local static ObjectList<T> obj_list_;
};

template <typename T>
thread_local ObjectList<T> ObjectPool<T>::obj_list_;

template <typename T>
T* GetObject() {
  return Singleton< ObjectPool<T> >::Instance()->Get();
}

template <typename T>
void FreeObject(T* obj) {
  Singleton< ObjectPool<T> >::Instance()->Free(obj);
}

};