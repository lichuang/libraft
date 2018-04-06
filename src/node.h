#ifndef __NODE_H__
#define __NODE_H__

#include "libraft.h"

enum NodeMessageType {
  ProposeMessage    = 0,
  RecvMessage       = 1,
  ConfChangeMessage = 2,
  TickMessage       = 3,
  ReadyMessage      = 4,
  NoneMessage       = 6
};

struct raft;

class NodeImpl : public Node {
public:
  NodeImpl(const Config *config);
  ~NodeImpl();

  virtual void Tick(Ready **ready);
  virtual int  Campaign(Ready **ready);
  virtual int  Propose(const string& data, Ready **ready);
  virtual int  ProposeConfChange(const ConfChange& cc, Ready **ready);
  virtual int  Step(const Message& msg, Ready **ready);
  virtual void Advance();
  virtual void ApplyConfChange(const ConfChange& cc, ConfState *cs, Ready **ready);
  virtual void TransferLeadership(uint64_t leader, uint64_t transferee, Ready **ready);
  virtual int  ReadIndex(const string &rctx, Ready **ready);

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
  raft *raft_;
  Logger *logger_;

  // save previous the state
  uint64_t leader_;
  SoftState prevSoftState_;
  HardState prevHardState_;
  bool waitAdvanced_;
  Ready ready_;
  bool canPropose_;
  NodeMessageType msgType_;

  uint64_t prevLastUnstableIndex_;
  uint64_t prevLastUnstableTerm_;
  bool     havePrevLastUnstableIndex_;
  uint64_t prevSnapshotIndex_;

  // for ApplyConfChange 
  ConfChange confChange_;
  ConfState*  confState_;
};

#endif  // __NODE_H__
