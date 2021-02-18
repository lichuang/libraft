#include "base/mutex.h"
namespace libraft {
struct LockerImpl {
  LockerImpl() {
  }
  ~LockerImpl() {
  }
  int Lock() {
    return 0;
  }
  int UnLock() {
    return 0;
  }
};

Locker::Locker() : impl_(new LockerImpl()) {
}

Locker::~Locker() {
  delete impl_;
}

int Locker::Lock() {
  return impl_->Lock();
}

int Locker::UnLock() {
  return impl_->UnLock();
}
}; // namespace libraft