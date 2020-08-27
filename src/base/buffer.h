/*
 * Copyright (C) lichuang
 */

#pragma once

#include <string.h>
#include "base/define.h"
#include "base/object_pool.h"
#include "core/log.h"

namespace libraft {

static const int kInitBufferSize = 4096;

class Buffer {
public:
  Buffer()
    : capacity_(kInitBufferSize),
      read_index_(0),
      write_index_(0) { 
    data_ = new char[capacity_];
    ASSERT(data_);   
    Reset();
  }

  ~Buffer() {
    delete data_;
  }

  // return start point of the buffer
  char* Data() {
    return &(data_[0]);
  }

  // return current buffer data length
  int Length() const {
    ASSERT(write_index_ >= read_index_);
    return write_index_ - read_index_;
  }

  size_t Capacity() const {
    return capacity_;
  }

  size_t WritableBytes() const {
    return capacity_ - write_index_;
  }

  size_t ReadableBytes() const {
    ASSERT(write_index_ >= read_index_);
    return write_index_ - read_index_;
  }

  void WriteAdvance(size_t len) {
    write_index_ += len;
    ASSERT(capacity_ >= write_index_);
  }

  void ReadAdvance(size_t len) {
    read_index_ += len;
    ASSERT(write_index_ >= read_index_);
  }

  // clear the data, set current point to start
  void Reset() {
    memset(data_, '0', capacity_);
    write_index_ = read_index_ = 0;
  }

  // return current point of the data
  char* Current() { return &data_[write_index_]; }

  char* WritePosition() { return &data_[write_index_]; }
  
  char* ReadPosition() { return &data_[read_index_]; }

  // append string to the buffer
  void AppendString(const string& str) {
    AppendData(str.c_str(), str.length());
  }

  // append the data to the buffer
  void AppendData(const char* buf, size_t len) {
    EnsureWritableBytes(len);

    memcpy(&data_[write_index_], buf, len);
    write_index_ += len;
  }

  const char* End() const { return &data_[capacity_]; }

  void EnsureWritableBytes(size_t len) {
    if (WritableBytes() < len) {
      grow(len);
    }
  }

private:
    void grow(size_t len) {
      /*
      if (WritableBytes() < len) {
        //grow the capacity
        size_t n = (capacity_ << 1) + len;
        size_t m = length();
        char* d = new char[n];
        memcpy(d, begin() + read_index_, m);
        write_index_ = m + reserved_prepend_size_;
        read_index_ = reserved_prepend_size_;
        capacity_ = n;
        delete[] buffer_;
        buffer_ = d;
      } else {
        // move readable data to the front, make space inside buffer
        assert(reserved_prepend_size_ < read_index_);
        size_t readable = length();
        memmove(begin() + reserved_prepend_size_, begin() + read_index_, length());
        read_index_ = reserved_prepend_size_;
        write_index_ = read_index_ + readable;
        assert(readable == length());
        assert(WritableBytes() >= len);
      }
      */
    }

private:
  char *data_;
  size_t capacity_;
  size_t read_index_;
  size_t write_index_;  

  DISALLOW_COPY_AND_ASSIGN(Buffer);
};

};