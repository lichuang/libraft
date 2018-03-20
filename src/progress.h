#ifndef __PROGRESS_H__
#define __PROGRESS_H__

#include "libraft.h"

struct inflights {
  // the starting index in the buffer
  int start_;

  // number of inflights in the buffer
  int count_;

  // the size of the buffer
  int size_;

  // buffer contains the index of the last entry
  // inside one message.
  vector<uint64_t> buffer_;

  Logger* logger_;

  void add(uint64_t infight);
  void growBuf();
  void freeTo(uint64_t to);
  void freeFirstOne();
  bool full();
  void reset();

  inflights(int size, Logger *logger)
    : start_(0),
      count_(0),
      size_(size),
      logger_(logger) {
    buffer_.resize(size);
  }
  ~inflights() {
  }
};

enum ProgressState {
  ProgressStateProbe     = 1,
  ProgressStateReplicate = 2,
  ProgressStateSnapshot  = 3
};

// Progress represents a follower’s progress in the view of the leader. Leader maintains
// progresses of all followers, and sends entries to the follower based on its progress.
struct Progress {
  uint64_t match_, next_;

  // State defines how the leader should interact with the follower.
  //
  // When in ProgressStateProbe, leader sends at most one replication message
  // per heartbeat interval. It also probes actual progress of the follower.
  //
  // When in ProgressStateReplicate, leader optimistically increases next
  // to the latest entry sent after sending replication message. This is
  // an optimized state for fast replicating log entries to the follower.
  //
  // When in ProgressStateSnapshot, leader should have sent out snapshot
  // before and stops sending any replication message.
  ProgressState state_;

  // Paused is used in ProgressStateProbe.
  // When Paused is true, raft should pause sending replication message to this peer.
  bool paused_;

  // PendingSnapshot is used in ProgressStateSnapshot.
  // If there is a pending snapshot, the pendingSnapshot will be set to the
  // index of the snapshot. If pendingSnapshot is set, the replication process of
  // this Progress will be paused. raft will not resend snapshot until the pending one
  // is reported to be failed.
  uint64_t pendingSnapshot_;

  // RecentActive is true if the progress is recently active. Receiving any messages
  // from the corresponding follower indicates the progress is active.
  // RecentActive can be reset to false after an election timeout.
  bool recentActive_;

  // inflights is a sliding window for the inflight messages.
  // Each inflight message contains one or more log entries.
  // The max number of entries per message is defined in raft config as MaxSizePerMsg.
  // Thus inflight effectively limits both the number of inflight messages
  // and the bandwidth each Progress can use.
  // When inflights is full, no more message should be sent.
  // When a leader sends out a message, the index of the last
  // entry should be added to inflights. The index MUST be added
  // into inflights in order.
  // When a leader receives a reply, the previous inflights should
  // be freed by calling inflights.freeTo with the index of the last
  // received entry.
  inflights ins_;
  Logger* logger_;

  const char* stateString();
  void resetState(ProgressState state);
  void becomeProbe();
  void becomeReplicate();
  void becomeSnapshot(uint64_t snapshoti);
  bool maybeUpdate(uint64_t n);
  void optimisticUpdate(uint64_t n);
  bool maybeDecrTo(uint64_t last, uint64_t rejected);
  void snapshotFailure();
  void pause();
  void resume();
  bool isPaused();
  bool needSnapshotAbort();
  string string();

  Progress(uint64_t next, int maxInfilght, Logger *logger);
  ~Progress();
};

#endif  // __PROGRESS_H__
