#include "raft.h"
#include "util.h"
#include "read_only.h"

HardState kEmptyState;

raft::raft(Config *config, raftLog *log)
  : id_(config->id),
    raftLog_(log),
    maxInfilght_(config->maxInflightMsgs),
    maxMsgSize_(config->maxSizePerMsg),
    leader_(None),
    leadTransferee_(None),
    readOnly_(new readOnly(config->readOnlyOption, config->logger)),
    heartbeatTimeout_(config->heartbeatTick),
    electionTimeout_(config->electionTick),
    logger_(config->logger) {
}

//TODO:
void validateConfig(Config *config) {

}

raft* newRaft(Config *config) {
  validateConfig(config);

  raftLog *rl = newLog(config->storage, config->logger);
  HardState hs;
  ConfState cs;
  Logger *logger = config->logger;
  vector<uint64_t> peers = config->peers;
  int err;
  size_t i;
  
  err = config->storage->InitialState(&hs, &cs);
  if (!SUCCESS(err)) {
    logger->Fatalf(__FILE__, __LINE__, "storage InitialState fail: %s", GetErrorString(err)); 
  }
  if (cs.nodes_size() > 0) {
    if (peers.size() > 0) {
      logger->Fatalf(__FILE__, __LINE__, "cannot specify both newRaft(peers) and ConfState.Nodes)");
    }
    peers.clear();
    for (i = 0; i < cs.nodes_size(); ++i) {
      peers.push_back(cs.nodes(i));
    }
  }

  raft *r = new raft(config, rl);
  for (i = 0; i < peers.size(); ++i) {
    r->prs_[peers[i]] = new Progress(1, r->maxInfilght_);
  }

  if (!isHardStateEqual(hs, kEmptyState)) {
    r->loadState(hs);
  }
  if (config->applied > 0) {
    rl->appliedTo(config->applied);
  }

  r->becomeFollower(r->term_, None);
  vector<string> peerStrs;
  map<uint64_t, Progress*>::const_iterator iter;
  char tmp[32];
  while (iter != r->prs_.end()) {
    snprintf(tmp, sizeof(tmp), "%llu", iter->first);
    peerStrs.push_back(tmp);
    ++iter;
  }
  string nodeStr = joinStrings(peerStrs, ",");

  r->logger_->Infof(__FILE__, __LINE__,
    "newRaft %llu [peers: [%s], term: %llu, commit: %llu, applied: %llu, lastindex: %llu, lastterm: %llu]",
    r->id_, nodeStr.c_str(), r->term_, rl->committed_, rl->applied_, rl->lastIndex(), rl->lastTerm());
  return r;
}

bool raft::hasLeader() {
  return leader_ != None;
}

void raft::softState(SoftState *ss) {
  ss->leader = leader_;
  ss->state  = state_;
}

void raft::hardState(HardState *hs) {
  hs->set_term(term_);
  hs->set_vote(vote_);
  hs->set_commit(raftLog_->committed_);
}

void raft::nodes(vector<uint64_t> *nodes) {
  map<uint64_t, Progress*>::const_iterator iter = prs_.begin();
  while (iter != prs_.end()) {
    nodes->push_back(iter->first);
    ++iter;
  }

  sort(nodes->begin(), nodes->end());
}

void raft::loadState(const HardState &hs) {
  if (hs.commit() < raftLog_->committed_ || hs.commit() > raftLog_->lastIndex()) {
    logger_->Fatalf(__FILE__, __LINE__, 
      "%x state.commit %llu is out of range [%llu, %llu]", id_, hs.commit(), raftLog_->committed_, raftLog_->lastIndex());
  }

  raftLog_->committed_ = hs.commit();
  term_ = hs.term();
  vote_ = hs.vote();
}

int raft::quorum() {
  return (prs_.size() / 2) + 1;
}

// send persists state to stable storage and then sends to its mailbox.
void raft::send(Message *msg) {
  msg->set_from(id_);
  int type = msg->type();

  // TODO: MsgPreVote
  if (type == MsgVote) {
    if (msg->term() == 0) {
      // PreVote RPCs are sent at a term other than our actual term, so the code
      // that sends these messages is responsible for setting the term.
      logger_->Fatalf(__FILE__, __LINE__, "term should be set when sending %s", msgTypeString(type));
    }
  } else {
    if (msg->term() != 0) { 
      logger_->Fatalf(__FILE__, __LINE__, "term should not be set when sending %s (was %llu)", msgTypeString(type), msg->term());
    }
    // do not attach term to MsgProp, MsgReadIndex
    // proposals are a way to forward to the leader and
    // should be treated as local message.
    // MsgReadIndex is also forwarded to leader.
    if (type != MsgProp && type != MsgReadIndex) {
      msg->set_term(term_);
    }
  }

  msgs_.push_back(msg);
}

// sendAppend sends RPC, with entries to the given peer.
void raft::sendAppend(uint64_t to) {
  Progress *pr = prs_[to];
  if (pr == NULL || pr->isPaused()) {
    return;
  }

  Message *msg = new Message();
  msg->set_to(to);

  uint64_t term;
  int errt, erre, err;
  EntryVec entries;
  Snapshot *snapshot;

  errt = raftLog_->term(pr->next_ - 1, &term);
  erre = raftLog_->entries(pr->next_, maxMsgSize_, &entries);
  if (!SUCCESS(errt) || !SUCCESS(erre)) {  // send snapshot if we failed to get term or entries
    if (!pr->recentActive_) {
      logger_->Debugf(__FILE__, __LINE__, "ignore sending snapshot to %llu since it is not recently active", to);
      return;
    }

    msg->set_type(MsgSnap);
    err = raftLog_->snapshot(&snapshot);
    if (!SUCCESS(err)) {
      if (err == ErrSnapshotTemporarilyUnavailable) {
        logger_->Debugf(__FILE__, __LINE__, "%llu failed to send snapshot to %llu because snapshot is temporarily unavailable", id_, to);
        return;
      }

      logger_->Fatalf(__FILE__, __LINE__, "get snapshot err: %s", GetErrorString(err));
    }
  
    if (isEmptySnapshot(snapshot)) {
      logger_->Fatalf(__FILE__, __LINE__, "need non-empty snapshot");
    }

    Snapshot *s = msg->mutable_snapshot();
    s->CopyFrom(*snapshot);
    uint64_t sindex = snapshot->metadata().index();
    uint64_t sterm = snapshot->metadata().term();
    logger_->Debugf(__FILE__, __LINE__, "%x [firstindex: %llu, commit: %llu] sent snapshot[index: %llu, term: %llu] to %x [%s]",
      id_, raftLog_->firstIndex(), raftLog_->committed_, sindex, sterm, to, pr->string().c_str());
    pr->becomeSnapshot(sindex);
    logger_->Debugf(__FILE__, __LINE__, "%x paused sending replication messages to %x [%s]", id_, to, pr->string().c_str());
  } else {
    msg->set_type(MsgApp);
    msg->set_index(pr->next_ - 1);
    msg->set_logterm(term);
    msg->set_commit(raftLog_->committed_);
    size_t i;
    for (i = 0; i < entries.size(); ++i) {
      Entry *entry = msg->add_entries();
      entry->CopyFrom(entries[i]);
    }
    if (entries.size() > 0) {
      uint64_t last;
      switch (pr->state_) {
      // optimistically increase the next when in ProgressStateReplicate
      case ProgressStateReplicate:
        last = entries[entries.size() - 1].index();
        pr->optimisticUpdate(last);
        pr->ins_->add(last);
        break;
      case ProgressStateProbe:
        pr->pause();
        break;
      default:
        logger_->Fatalf(__FILE__, __LINE__, "%x is sending append in unhandled state %s", id_, pr->stateString());
        break;
      }
    }
  }

  send(msg);
}

// sendHeartbeat sends an empty MsgApp
void raft::sendHeartbeat(uint64_t to, const string &ctx) {
  // Attach the commit as min(to.matched, r.committed).
  // When the leader sends out heartbeat message,
  // the receiver(follower) might not be matched with the leader
  // or it might not have all the committed entries.
  // The leader MUST NOT forward the follower's commit to
  // an unmatched index.
  uint64_t commit = min(prs_[to]->match_, raftLog_->committed_);
  Message *msg = new Message();
  msg->set_to(to);
  msg->set_type(MsgHeartbeat);
  msg->set_commit(commit);
  msg->set_context(ctx);
  send(msg);
}

// bcastAppend sends RPC, with entries to all peers that are not up-to-date
// according to the progress recorded in r.prs.
void raft::bcastAppend() {
  map<uint64_t, Progress*>::const_iterator iter = prs_.begin();
  for (;iter != prs_.end();++iter) {
    if (iter->first == id_) {
      continue;
    }
    sendAppend(iter->first);
  }
}

// bcastHeartbeat sends RPC, without entries to all the peers.
void raft::bcastHeartbeatWithCtx(const string &ctx) {
  map<uint64_t, Progress*>::const_iterator iter = prs_.begin();
  for (;iter != prs_.end();++iter) {
    if (iter->first == id_) {
      continue;
    }
    sendHeartbeat(iter->first, ctx);
  }
}

// maybeCommit attempts to advance the commit index. Returns true if
// the commit index changed (in which case the caller should call
// r.bcastAppend).
bool raft::maybeCommit() {
}

void raft::reset(uint64_t term) {
  if (term_ != term) {
    term_ = term;
    vote_ = None;
  }
  leader_ = None;

  electionElapsed_ = 0;
  heartbeatElapsed_ = 0;
  resetRandomizedElectionTimeout();

  abortLeaderTransfer();
  votes_.clear();
  map<uint64_t, Progress*>::iterator iter = prs_.begin();
  for (; iter != prs_.end(); ++iter) {
    uint64_t id = iter->first;
    Progress *pr = prs_[id];
    delete pr;
    prs_[id] = new Progress(raftLog_->lastIndex() + 1, new inflights(maxInfilght_));
    if (id == id_) {
      pd = prs_[id];
      pr->match_ = raftLog_->lastIndex();
    }
  }
  pendingConf_ = false;
  readOnly_ = new readOnly(readOnly_->option_);
}

void raft::appendEntry(EntryVec* entries) {
  uint64_t li = raftLog_->lastIndex();
  size_t i;
  for (i = 0; i < entries->size(); ++i) {
    (*entries)[i].set_term(term_);
    (*entries)[i].set_index(li + 1 + i);
  }
  raftLog_->append(*entries);
  prs_[id_]->maybeUpdate(raftLog_->lastIndex());
  maybeCommit();
}

// tickElection is run by followers and candidates after r.electionTimeout.
void raft::tickElection() {
  electionElapsed_++;
  
  if (promotable() && pastElectionTimeout()) {
    electionElapsed_ = 0;
    Message *msg = new Message();
    msg->set_from(id_);
    msg->set_type(MsgHup);
    step(msg);
  }
}

// tickHeartbeat is run by leaders to send a MsgBeat after r.heartbeatTimeout.
void raft::tickHeartbeat() {
  heartbeatElapsed_++;
  electionElapsed_++;

  if (electionElapsed_ >- electionTimeout_) {
    electionElapsed_ = 0;
    if (checkQuorum_) {
      Message *msg = new Message();
      msg->set_from(id_);
      msg->set_type(MsgCheckQuorum);
      step(msg);
    }
    // If current leader cannot transfer leadership in electionTimeout, it becomes leader again.
    if (state_ == StateLeader && leadTransferee_ != None) {
      abortLeaderTransfer();
    }
  }

  if (state_ != StateLeader) {
    return;
  }

  if (heartbeatElapsed_ >= heartbeatTimeout_) {
    heartbeatElapsed_ = 0;

    Message *msg = new Message();
    msg->set_from(id_);
    msg->set_type(MsgBeat);
    step(msg);
  }
}

// promotable indicates whether state machine can be promoted to leader,
// which is true when its own id is in progress list.
bool raft::promotable() {
  return prs_.find(id_) != prs_.end();
}

// pastElectionTimeout returns true iff r.electionElapsed is greater
// than or equal to the randomized election timeout in
// [electiontimeout, 2 * electiontimeout - 1].
bool raft::pastElectionTimeout() {
  return electionElapsed_ >- randomizedElectionTimeout_;
}

void raft::resetRandomizedElectionTimeout() {
}

void raft::becomeFollower(uint64_t term, uint64_t leader) {
  step_ = stepFollower;
  reset(term);
  tick_  = tickElection;
  leader_ = leader;
  state_ = StateFollower;
  logger_->Infof(__FILE__, __LINE__, "%x became follower at term %llu", id_, term_);
}

void raft::becomeCandidate() {
  if (state_ == StateLeader) {
    logger_->Fatalf(__FILE__, __LINE__, "invalid transition [leader -> candidate]")
  }

  step_ = stepCandidate;
  tick_ = tickElection;
  vote_ = id_;
  state_ = StateCandidate;
  logger_->Infof(__FILE__, __LINE__, "%x became candidate at term %llu", id_, term_);
}

void raft::becomeLeader() {
  if (state_ == StateFollower) {
    logger_->Fatalf(__FILE__, __LINE__, "invalid transition [follower -> leader]")
  }

  step_ = stepLeader;
  reset(term_);
  tick_ = tickHeartbeat;
  leader_ = id_;
  state_ = StateLeader;

  EntryVec entries;
  int err = raftLog_->entries(raftLog_->committed_ + 1, noLimit, &entries);
  if (!SUCCESS(err)) {
    logger_->Fatalf(__FILE__, __LINE__, "unexpected error getting uncommitted entries (%s)", GetErrorString(err));
  }

  int n = numOfPendingConf(entries);
  if (n > 1) {
    logger_->Fatalf(__FILE__, __LINE__, "unexpected multiple uncommitted config entry");
  }
  if (n == 1) {
    pendingConf_ = true;
  }
  entries.clear();
  entries.push_back(Entry());
  appendEntry(&entries);
  logger_->Infof(__FILE__, __LINE__, "%x became leader at term %llu", id_, term_);
}

void raft::campaign(CampaignType t) {
  uint64_t term;
  MessageType voteMsg;
  if (t == campaignPreElection) {
    // TODO
  } else {
    becomeCandidate();
    voteMsg = MsgVote;
    term = term_;
  }

  if (quorum() == poll(id_, voteRespMsgType(voteMsg), true)) {
    // We won the election after voting for ourselves (which must mean that
    // this is a single-node cluster). Advance to the next state.
    if (t == campaignPreElection) {
      // TODO
    } else {
      becomeLeader();
    }
  }

  size_t i;
  map<uint64_t, Progress*>::const_iterator iter = prs_.begin();
  for (; iter != prs_.end(); ++iter) {
    uint64_t id = iter->first;
    if (id_ == id) {
      continue;
    }
    logger_->Infof(__FILE__, __LINE__, "%x [logterm: %llu, index: %llu] sent %s request to %x at term %llu",
      id_, raftLog_->lastTerm(), raftLog_->lastIndex(), getCampaignString(t), id, term_);
    string ctx;
    if (t == campaignTransfer) {
      ctx = getCampaignString(t);
    }
    Message *msg = new Message();
    msg->set_term(term);
    msg->set_to(to);
    msg->set_type(voteMsg);
    msg->set_index(raftLog_->lastIndex());
    msg->set_logterm(raftLog_->lastTerm());
    msg->set_context(ctx);
    step(msg);
  }
}

int raft::poll(uint64_t id, MessageType t, bool v) {
  if (v) {
    logger_->Infof(__FILE__, __LINE__, "%x received %s from %x at term %d", id_, t, id, term_);
  } else {
    logger_->Infog(__FILE__, __LINE__, "%x received %s rejection from %x at term %d", id_, t, id, term_);
  }
  if (votes_.find(id) == votes_.end()) {
    votes_[id] = v;
  }
  map<uint64_t, bool>::const_iterator iter = votes_.begin();
  int granted = 0;
  for (; iter != votes_.end(); ++iter) {
    if (iter->second) {
      granted++;
    }
  }
  return granted;
}

void raft::abortLeaderTransfer() {
  leadTransferee_ = None;
}
