/*
 * Copyright (C) lichuang
 */

#include <sys/types.h>
#include <unistd.h>
#include "util/global_id.h"
#include "util/system.h"

extern uint64_t hash3(uint8_t*, uint64_t, uint64_t);

namespace libraft {
extern uint64_t CurrentMs();

static inline uint64_t 
hash_data(const uint8_t*s, uint64_t size, uint64_t key) {
  return ::hash3(const_cast<uint8_t*>(s), size, key);
}

struct idGenerator {
  idGenerator()
    : initialized_(false) {
  }

  void init();

  uint64_t NewID();

  uint64_t newRandom();

  void reset();

  uint64_t upper_bits_;
  uint64_t seed_;
  int lower_bits_;

  int increment_;

  int remaining_;

  bool initialized_;
};

static const int kLog2NumIDsInBatch = 12;
static const int kNumIDsInBatch = 1 << kLog2NumIDsInBatch;
static const int kBatchMask = kNumIDsInBatch - 1;

thread_local idGenerator gIDGenerator;

uint64_t
idGenerator::NewID() {
  if (remaining_ == 0) {
    reset();
  }

  --remaining_;
  lower_bits_ = (lower_bits_ + increment_) & kBatchMask;
  return upper_bits_ | lower_bits_;
}

void
idGenerator::init() {
  initialized_ = true;
  string *hostname = GetHostName();

  seed_ =
    hash_data(reinterpret_cast<const uint8_t*>(hostname->c_str()),
          hostname->length(),
          time(NULL) + getpid() + pthread_self());
}

uint64_t
idGenerator::newRandom() {
  if (!initialized_) {
    init();
  }

  uint64_t now = CurrentMs();
  seed_ = hash_data(
      reinterpret_cast<const uint8_t*>(&now),
      sizeof(now), seed_);
  return seed_;
}

void
idGenerator::reset() {
  uint64_t v = newRandom();

  upper_bits_ = v & ~kBatchMask;
  lower_bits_ = v & kBatchMask;

  increment_ = newRandom() & kBatchMask;

  increment_ |= 1;

  remaining_ = kNumIDsInBatch;
}

id_t 
NewGlobalID() {
	return gIDGenerator.NewID();
}
};