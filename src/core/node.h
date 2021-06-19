/*
 * Copyright (C) lichuang
 */

#ifndef __LIBRAFT_NODE_H__
#define __LIBRAFT_NODE_H__

#include "libraft.h"

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

class NodeImpl : public Node {
public:
  NodeImpl(raft*);
  virtual ~NodeImpl();

  virtual void Tick(Ready **ready);
  virtual int  Campaign(Ready **ready);
  virtual int  Propose(const string& data, Ready **ready);
  virtual int  ProposeConfChange(const ConfChange& cc, Ready **ready);
  virtual int  Step(const Message& msg, Ready **ready);
  virtual void Advance();
  virtual void ApplyConfChange(const ConfChange& cc, ConfState *cs, Ready **ready);
  virtual void TransferLeadership(uint64_t leader, uint64_t transferee, Ready **ready);
  virtual int  ReadIndex(const string &rctx, Ready **ready);
  virtual void Stop();

private:
  int stateMachine(const Message& msg, Ready **ready);
  Ready* newReady();
  int doStep(const Message& msg, Ready **ready);
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
