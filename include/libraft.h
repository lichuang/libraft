/*
 * Copyright (C) lichuang
 */

#ifndef __LIB_RAFT_H__
#define __LIB_RAFT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

typedef uint64_t raft_term_t;
typedef uint64_t raft_index_t;
typedef uint64_t peer_t;

typedef struct bytes_t {
  uint32_t len;
  char data[0];
} bytes_t;

typedef enum entry_e {
  EntryNormal     = 0,
  EntryConfChange = 1,
} entry_e;

typedef struct entry_t {
  entry_e type;
  raft_term_t term;
  raft_index_t index;
  bytes_t data;
} entry_t;

typedef struct conf_state_t {
  uint16_t len;
  peer_t nodes[0];
} conf_state_t;

typedef struct snapshot_meta_t {
  conf_state_t conf_state;
  raft_index_t index;
  raft_term_t term;
} snapshot_meta_t;

typedef struct snapshot_t {
  bytes_t data;
  snapshot_meta_t meta;
} snapshot_t;

typedef enum message_e {
  MsgHup             = 0,
  MsgBeat            = 1,
  MsgProp            = 2,
  MsgApp             = 3,
  MsgAppResp         = 4,
  MsgVote            = 5,
  MsgVoteResp        = 6,
  MsgSnap            = 7,
  MsgHeartbeat       = 8,
  MsgHeartbeatResp   = 9,
  MsgUnreachable     = 10,
  MsgSnapStatus      = 11,
  MsgCheckQuorum     = 12,
  MsgTransferLeader  = 13,
  MsgTimeoutNow      = 14,
  MsgReadIndex       = 15,
  MsgReadIndexResp   = 16,
  MsgPreVote         = 17,
  MsgPreVoteResp     = 18,
} message_e;

typedef struct hard_state_t {
  raft_term_t term;
  peer_t vote;
  raft_index_t commit;
} hard_state_t;

typedef enum conf_change_e {
  ConfChangeAddNode    = 0,
  ConfChangeRemoveNode = 1,
  ConfChangeUpdateNode = 2,
} conf_change_e;

typedef struct conf_change_t {
  uint64_t id;
  conf_change_e type;
  peer_t node_id;
  bytes_t context;
} conf_change_t;

typedef struct message_t {
  message_e type;
  bytes_t data;
} message_t;

const static peer_t kEmptyPeerId = 0;
const static uint64_t kNoLimit = ULONG_MAX;

typedef enum error_e{
  OK                                = 0,

  // ErrCompacted is returned by Storage.Entries/Compact when a requested
  // index is unavailable because it predates the last snapshot.  
  ErrCompacted                      = 1,

  // ErrSnapOutOfDate is returned by Storage.CreateSnapshot when a requested
  // index is older than the existing snapshot.  
  ErrSnapOutOfDate                  = 2,

  // ErrUnavailable is returned by Storage interface when the requested log entries
  // are unavailable.  
  ErrUnavailable                    = 3,

  // ErrSnapshotTemporarilyUnavailable is returned by the Storage interface when the required
  // snapshot is temporarily unavailable.  
  ErrSnapshotTemporarilyUnavailable = 4,

  // ErrSerializeFail is returned by the Node interface when the request data Serialize failed. 
  ErrSerializeFail                  = 5,

  // Number of error code
  NumErrorCode
} error_e;

inline bool SUCCESS(int err) { return err == OK; }

typedef enum state_e {
  StateFollower = 0,
  StateCandidate = 1,
  StateLeader = 2,
  StatePreCandidate = 3,
  NumStateType
} state_e;

typedef struct soft_state_t {
  peer_t leader;
  state_e state;
} soft_state_t;

// read_state_t provides state for read only query.
// It's caller's responsibility to call ReadIndex first before getting
// this state from ready, it's also caller's duty to differentiate if this
// state is what it requests through request_context, eg. given a unique id as
// read_state_t
typedef struct read_state_t {
  raft_index_t index;
  bytes_t request_context;
} read_state_t;


#ifdef __cplusplus
}
#endif

#endif  // __LIB_RAFT_H__