/*
 * Copyright (C) lichuang
 */

#ifndef __LIBRAFT_NODE_H__
#define __LIBRAFT_NODE_H__

#include "libraft.h"
#include "core/fsm_caller.h"

namespace libraft {

enum NodeMessageType {
  ProposeMessage    = 0,
  RecvMessage       = 1,
  ConfChangeMessage = 2,
  TickMessage       = 3,
  ReadyMessage      = 4,
  NoneMessage       = 5
};

struct raft;

struct Ready {
 	// The current volatile state of a Node.
	// SoftState will be nil if there is no update.
	// It is not required to consume or store SoftState.
  SoftState         softState;

	// The current state of a Node to be saved to stable storage BEFORE
	// Messages are sent.
	// HardState will be equal to empty state if there is no update.
  HardState         hardState;

 	// ReadStates can be used for node to serve linearizable read requests locally
	// when its applied index is greater than the index in ReadState.
	// Note that the readState will be returned when raft receives msgReadIndex.
	// The returned is only valid for the request that requested to read.
  vector<ReadState*> readStates;

	// Entries specifies entries to be saved to stable storage BEFORE
	// Messages are sent.
  EntryVec          entries;

  // Snapshot specifies the snapshot to be saved to stable storage.
  Snapshot          *snapshot;

	// CommittedEntries specifies entries to be committed to a
	// store/state-machine. These have previously been committed to stable
	// store.
  EntryVec          committedEntries;

 	// Messages specifies outbound messages to be sent AFTER Entries are
	// committed to stable storage.
	// If it contains a MsgSnap message, the application MUST report back to raft
	// when the snapshot has been received or has failed by calling ReportSnapshot.
  MessageVec  messages;
};

class NodeImpl : public Node {
public:
  NodeImpl(raft*, Config*);
  virtual ~NodeImpl();

  virtual void Tick();
  virtual int  Campaign();
  virtual int  Propose(const string& data);
  virtual int  ProposeConfChange(const ConfChange& cc);
  virtual int  Step(const Message& msg);
  virtual void Advance();
  virtual void ApplyConfChange(const ConfChange& cc, ConfState *cs);
  virtual void TransferLeadership(uint64_t leader, uint64_t transferee);
  virtual int  ReadIndex(const string &rctx);
  virtual void Stop();

  Ready* get_ready() {
    return &ready_;
  }

private:
  int stateMachine(const Message& msg);
  void newReady();
  int doStep(const Message& msg);
  bool isMessageFromClusterNode(const Message& msg);
  void handleConfChange();
  void handleAdvance();
  void reset();
  bool readyContainUpdate();

public:
  bool stopped_;

  // the Raft state machine
  raft *raft_;

  // save previous the state
  uint64_t leader_;
  SoftState prevSoftState_;
  HardState prevHardState_;
  bool waitAdvanced_;

  // save Ready data in each step
  Ready ready_;

  FsmCaller fsm_caller_;

  // if there is no leader, then cannot propose any msg
  bool canPropose_;

  // save state machine msg type
  NodeMessageType msgType_;

  // save previous storage data, in `Advance' func, use these datas to update storage
  uint64_t prevLastUnstableIndex_;
  uint64_t prevLastUnstableTerm_;
  bool     havePrevLastUnstableIndex_;
  uint64_t prevSnapshotIndex_;

  // for ApplyConfChange 
  ConfChange confChange_;
  ConfState*  confState_;
};

}; // namespace libraft

#endif  // __LIBRAFT_NODE_H__
