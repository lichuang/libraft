/*
 * Copyright (C) lichuang
 */

#pragma once

#include <string.h>
#include "base/define.h"
#include "base/object_pool.h"
#include "base/log.h"

namespace libraft {

static const size_t kPacketSize = 4096;

class Buffer {
  friend struct Packet;
public:
  Buffer() {
    begin_ = new Packet(this);
    read_ = begin_;
    write_ = begin_;
  }

  ~Buffer() {
    Packet *ptr = begin_;
    while (ptr) {
      Packet *packet = ptr->next_;
      delete ptr;
      ptr = packet;
    }
  }

  size_t Length() const {
    size_t len = 0;
    Packet *ptr = begin_;
    while (ptr) {
      len += ptr->Length();
      ptr = ptr->next_;
    }
    return len;
  }

  bool Empty() const {
    return read_->Empty();
  }

  bool Full() const {
    return write_->Full();
  }

  size_t WritableBytes() const {
    return write_->WritableBytes();
  }
  
  size_t ReadableBytes() const {
    return read_->ReadableBytes();
  }

  void WriteAdvance(size_t len) {
    write_->WriteAdvance(len);    
  }

  void ReadAdvance(size_t len) {
    read_->ReadAdvance(len);
  }

  void Write(const char* data, int len) {
    write_->Write(data, len);
  }

  size_t Read(char* to, size_t n) {
    size_t ret = 0;
    while (read_ && !read_->Empty() && n > 0) {
      size_t len = read_->Read(to, n);
      n -= len;
      ret += len;
    }

    return ret;
  }

  char* WritePosition() { return write_->WritePosition(); }
  
  char* ReadPosition() { return read_->ReadPosition(); }

  // append string to the buffer
  void AppendString(const string& str) {    
    AppendData(str.c_str(), str.length());
  }

  // append the data to the buffer
  void AppendData(const char* buf, size_t len) {    
    EnsureWritableBytes(len);

    write_->Write(buf, len);
  }

  void EnsureWritableBytes(size_t len) {
    if (write_->WritableBytes() < len) {
      grow();
    }
  }

private:
    void grow() {
      Packet *p = new Packet(this);
      write_->next_ = p;
      write_ = p;
    }

    void next() {
      if (read_ != write_) {
        read_ = write_;
      }
    }

  struct Packet {
    Packet(Buffer* buffer)
      : buffer_(buffer),
        next_(nullptr),
        read_index_(0),
        write_index_(0) {}

    size_t Length() const {
      return write_index_;
    }

    char* WritePosition() { return &data_[write_index_]; }

    char* ReadPosition() { return &data_[read_index_]; }

    size_t WritableBytes() const {
      return kPacketSize - write_index_;
    }

    size_t ReadableBytes() const {
      return write_index_ - read_index_;
    }

    bool Empty() const {
      return write_index_ == read_index_;
    }

    bool Full() const {
      return write_index_ == kPacketSize;
    }

    void checkFull() {
      if (Full()) {
        buffer_->grow();
      }
    }

    void checkEmpty() {
      if (Empty()) {
        buffer_->next();
      }
    }

    void Write(const char* data, int len) {
      ASSERT(kPacketSize >= write_index_ + len);
      memcpy(&data_[write_index_], data, len);
      WriteAdvance(len);     
    }

    void WriteAdvance(size_t len) {
      write_index_ += len;
      ASSERT(kPacketSize >= write_index_);
      checkFull();
    }

    size_t Read(char* to, size_t n) {
      size_t readLen = ReadableBytes() >= n ? n : ReadableBytes();
      memcpy(to, &data_[read_index_], readLen);
      ReadAdvance(readLen);
      return readLen;
    }

    void ReadAdvance(size_t len) {
      read_index_ += len;
      ASSERT(kPacketSize >= read_index_);
      checkEmpty();
    }

    Buffer* buffer_;
    Packet *next_;
    size_t read_index_;
    size_t write_index_; 
    char data_[kPacketSize];  
  };

private:
  Packet *begin_;
  Packet *read_, *write_;
  DISALLOW_COPY_AND_ASSIGN(Buffer);
};

};