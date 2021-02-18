#ifndef __MUTEX_H__
#define __MUTEX_H__
namespace libraft {
struct LockerImpl;

class Locker {
public:
  Locker();
  ~Locker();

  int Lock();
  int UnLock();
private:
  LockerImpl *impl_;
};

class Mutex {
public:
  Mutex(Locker *locker) : locker_(locker) {
    locker_->Lock();
  }
  ~Mutex() {
    locker_->UnLock();
  }
private:
  Locker *locker_;
};
}; // namespace libraft
#endif  // __MUTEX_H__
