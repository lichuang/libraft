#include "node.h"
#include "raft.h"
#include "util.h"
#include <unistd.h>

const static HardState kEmptyHardState;
const static SoftState kEmptySoftState;
const static Snapshot  kEmptySnapshot;

// IsEmptySnap returns true if the given Snapshot is empty.
static bool isEmptyHardState(const HardState& hs) {
  return isHardStateEqual(hs, kEmptyHardState);
}

static bool isEmptySoftState(const SoftState& ss) {
  return isSoftStateEqual(ss, kEmptySoftState);
}

NodeImpl::NodeImpl()
  : stopped_(false)
  , raft_(NULL)
  , logger_(NULL)
  , leader_(None)
  , prevSoftState_(kEmptySoftState)
  , prevHardState_(kEmptyHardState)
  , waitAdvanced_(false)
  , canPropose_(true)
  , msgType_(NoneMessage)
  , prevLastUnstableIndex_(0)
  , prevLastUnstableTerm_(0)
  , havePrevLastUnstableIndex_(false)
  , prevSnapshotIndex_(0)
  , confState_(NULL) {
}

NodeImpl::~NodeImpl() {
  delete raft_;
}

void NodeImpl::Tick(Ready **ready) {
  msgType_ = TickMessage;
  stateMachine(Message(), ready);
}

int NodeImpl::Campaign(Ready **ready) {
  Message msg;
  msg.set_type(MsgHup);
  return doStep(msg, ready);
}

int NodeImpl::Propose(const string& data, Ready **ready) {
  Message msg;
  msg.set_type(MsgProp);
  msg.set_from(raft_->id_);
  msg.add_entries()->set_data(data);
  return doStep(msg, ready);
}

int NodeImpl::ProposeConfChange(const ConfChange& cc, Ready **ready) {
  string data;
  if (!cc.SerializeToString(&data)) {
    logger_->Errorf(__FILE__, __LINE__, "ConfChange SerializeToString error");
    return ErrSeriaFail;
  }

  Message msg;
  msg.set_type(MsgProp);
  msg.set_from(raft_->id_);
  Entry *entry = msg.add_entries();
  entry->set_type(EntryConfChange);
  entry->set_data(data);

  return Step(msg, ready);
}

int NodeImpl::Step(const Message& msg, Ready **ready) {
  // ignore unexpected local messages receiving over network
  if (isLoaclMessage(msg.type())) {
    *ready = NULL;
    return OK;
  }

  return doStep(msg, ready);
}

void NodeImpl::Advance() {
  if (prevHardState_.commit() != 0) {
    raft_->raftLog_->appliedTo(prevHardState_.commit());
  }
  if (havePrevLastUnstableIndex_) {
    raft_->raftLog_->stableTo(prevLastUnstableIndex_, prevLastUnstableTerm_);
    havePrevLastUnstableIndex_ = false;
  }
  raft_->raftLog_->stableSnapTo(prevSnapshotIndex_);
  int i;
  for (i = 0; i < ready_.messages.size(); ++i) {
    delete ready_.messages[i];
  }
  for (i = 0; i < ready_.readStates.size(); ++i) {
    delete ready_.readStates[i];
  }
  waitAdvanced_ = false;
}

void NodeImpl::ApplyConfChange(const ConfChange& cc, ConfState *cs, Ready **ready) {
  confChange_ = cc;
  confState_ = cs;
  /*
  msgType_ = ConfChangeMessage;
  stateMachine(Message(), ready);
  */
  *ready = NULL;
  handleConfChange();
}

int NodeImpl::doStep(const Message& msg, Ready **ready) {
  if (msg.type() == MsgProp) {
    msgType_ = ProposeMessage;
  } else {
    msgType_ = RecvMessage;
  }

  return stateMachine(msg, ready);
}

void NodeImpl::TransferLeadership(uint64_t leader, uint64_t transferee, Ready **ready) {
  msgType_ = RecvMessage;
  Message msg;
  msg.set_type(MsgTransferLeader);
  msg.set_from(transferee);
  msg.set_to(leader);

  stateMachine(msg, ready);
}

int NodeImpl::ReadIndex(const string &rctx, Ready **ready) {
  Message msg;
  msg.set_type(MsgReadIndex);
  msg.add_entries()->set_data(rctx);

  return doStep(msg, ready);
}

int NodeImpl::stateMachine(const Message& msg, Ready **ready) {
  if (stopped_) {
    return OK;
  }
  if (leader_ != raft_->leader_) {
    if (raft_->hasLeader()) {
      if (leader_ == None) {
        logger_->Infof(__FILE__, __LINE__, "raft.node: %x elected leader %x at term %llu",
          raft_->id_, raft_->leader_, raft_->term_);
      } else {
        logger_->Infof(__FILE__, __LINE__, "raft.node: %x changed leader from %x to %x at term %llu",
          raft_->id_, leader_, raft_->leader_, raft_->term_);
      }
      canPropose_ = true;
    } else {
      canPropose_ = false;
      logger_->Infof(__FILE__, __LINE__, "raft.node: %x lost leader %x at term %llu",
        raft_->id_, leader_, raft_->term_);
    }
    leader_ = raft_->leader_;
  }

  int ret = OK;
  switch (msgType_) {
  case ProposeMessage:
    if (canPropose_) {
      raft_->step(msg);
    }
    break;
  case RecvMessage:
    // filter out response message from unknown From.
    if (isMessageFromClusterNode(msg) || !isResponseMessage(msg.type())) {
      raft_->step(msg);
    }
    break;
  case TickMessage:
    raft_->tick();
    break;
  case ConfChangeMessage:
    handleConfChange();
    break;
  default:
    break;
  }

  if (waitAdvanced_) {
    *ready = NULL;
  } else {
    *ready = newReady();
    if (!readyContainUpdate()) {
      *ready = NULL;
    } else {
      waitAdvanced_ = true;
    }
  }

  reset();
  return ret;
}

void NodeImpl::handleConfChange() {
  if (confChange_.nodeid() == None) {
    raft_->resetPendingConf();
    goto addnodes;
  }

  switch(confChange_.type()) {
  case ConfChangeAddNode:
    raft_->addNode(confChange_.nodeid());
    break;
  case ConfChangeRemoveNode:
    // block incoming proposal when local node is removed
    if (confChange_.nodeid() == raft_->id_) {
      canPropose_ = false;
    }
    raft_->removeNode(confChange_.nodeid());
    break;
  case ConfChangeUpdateNode:
    raft_->resetPendingConf();
    break;
  default:
    logger_->Fatalf(__FILE__, __LINE__, "unexpected conf type");
    break;
  }

addnodes:  
  vector<uint64_t> nodes;
  raft_->nodes(&nodes);
  int j;
  for (j = 0; j < nodes.size(); ++j) {
    confState_->add_nodes(nodes[j]);
  }
}

void NodeImpl::reset() {
  msgType_ = NoneMessage;
  confState_ = NULL;
}

bool NodeImpl::isMessageFromClusterNode(const Message& msg) {
  return (raft_->prs_.find(msg.from()) != raft_->prs_.end());
}

Ready* NodeImpl::newReady() {
  // 1) reset ready data
  ready_.softState = kEmptySoftState;
  ready_.hardState = kEmptyHardState;
  ready_.snapshot  = NULL;
  ready_.readStates.clear();
  ready_.entries.clear();
  ready_.committedEntries.clear();
  ready_.messages.clear();
  
  // 2) return the new ready state data in ready
  raft_->raftLog_->unstableEntries(&ready_.entries);
  raft_->raftLog_->nextEntries(&ready_.committedEntries);
  ready_.messages = raft_->msgs_;

  SoftState ss;
  raft_->softState(&ss);
  if (!isSoftStateEqual(ss, prevSoftState_)) {
    ready_.softState = ss;
  }

  HardState hs;
  raft_->hardState(&hs);
  if (!isHardStateEqual(hs, prevHardState_)) {
    ready_.hardState = hs;
  }

  if (raft_->raftLog_->unstable_.snapshot_ != NULL) {
    ready_.snapshot = raft_->raftLog_->unstable_.snapshot_;
  }

  if (!raft_->readStates_.empty()) {
    ready_.readStates = raft_->readStates_;
  }

  // 3) save the state data
  prevSoftState_ = ready_.softState;
  size_t entSize = ready_.entries.size();
  if (entSize > 0) {
    prevLastUnstableIndex_ = ready_.entries[entSize - 1].index();
    prevLastUnstableTerm_  = ready_.entries[entSize - 1].term();
    havePrevLastUnstableIndex_ = true;
  }
  if (!isEmptyHardState(ready_.hardState)) {
    prevHardState_ = ready_.hardState;
  }
  if (!isEmptySnapshot(ready_.snapshot)) {
    prevSnapshotIndex_ = ready_.snapshot->metadata().index();
  }

  raft_->msgs_.clear();
  raft_->readStates_.clear();

  return &ready_;
}

void NodeImpl::Stop() {
  stopped_ = true;
}

bool NodeImpl::readyContainUpdate() {
  return (!isEmptySoftState(ready_.softState) ||
          !isEmptyHardState(ready_.hardState) ||
          !isEmptySnapshot(ready_.snapshot)  ||
          !ready_.entries.empty()             ||
          !ready_.committedEntries.empty()    ||
          !ready_.messages.empty()            ||
          !ready_.readStates.empty());
}

// StartNode returns a new Node given configuration and a list of raft peers.
// It appends a ConfChangeAddNode entry for each given peer to the initial log.
Node* StartNode(const Config* config, const vector<Peer>& peers) {
  NodeImpl* node = new NodeImpl();
  node->logger_ = config->logger;
  raft *r = newRaft(config);
  
	// become the follower at term 1 and apply initial configuration
	// entries of term 1
  r->becomeFollower(1, None);

  int i;
  for (i = 0; i < peers.size(); ++i) {
    const Peer& peer = peers[i];

    ConfChange cc;
    cc.set_type(ConfChangeAddNode);
    cc.set_nodeid(peer.Id);
    cc.set_context(peer.Context);
    string str;
    cc.SerializeToString(&str);

    Entry entry;
    EntryVec entries;
    entry.set_type(EntryConfChange);
    entry.set_term(1);
    entry.set_index(r->raftLog_->lastIndex() + 1);
    entry.set_data(str);
    entries.push_back(entry);

    r->raftLog_->append(entries);
  }
	// Mark these initial entries as committed.
	// TODO(bdarnell): These entries are still unstable; do we need to preserve
	// the invariant that committed < unstable?
  r->raftLog_->committed_ = r->raftLog_->lastIndex();
 	// Now apply them, mainly so that the application can call Campaign
	// immediately after StartNode in tests. Note that these nodes will
	// be added to raft twice: here and when the application's Ready
	// loop calls ApplyConfChange. The calls to addNode must come after
	// all calls to raftLog.append so progress.next is set after these
	// bootstrapping entries (it is an error if we try to append these
	// entries since they have already been committed).
	// We do not set raftLog.applied so the application will be able
	// to observe all conf changes via Ready.CommittedEntries.
  for (i = 0; i < peers.size(); ++i) {
    const Peer& peer = peers[i];
    r->addNode(peer.Id);
  }

  node->raft_ = r;
  r->softState(&node->prevSoftState_);

  return node;
}

// RestartNode is similar to StartNode but does not take a list of peers.
// The current membership of the cluster will be restored from the Storage.
// If the caller has an existing state machine, pass in the last log index that
// has been applied to it; otherwise use zero.
Node* RestartNode(const Config *config) {
  raft *r = newRaft(config);
  NodeImpl* node = new NodeImpl();
  node->logger_ = config->logger;
  node->raft_ = r;
  r->softState(&node->prevSoftState_);

  return node;
}
