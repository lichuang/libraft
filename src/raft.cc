#include <stdlib.h>
#include <time.h>
#include <algorithm>
#include "raft.h"
#include "util.h"
#include "read_only.h"

HardState kEmptyState;
const static string kCampaignPreElection = "CampaignPreElection";
const static string kCampaignElection = "CampaignElection";
const static string kCampaignTransfer = "CampaignTransfer";

static const char* msgTypeString(int t) {
  if (t == MsgHup) return "MsgHup";
  if (t == MsgBeat) return "MsgBeat";
  if (t == MsgProp) return "MsgProp";
  if (t == MsgApp) return "MsgApp";
  if (t == MsgAppResp) return "MsgAppResp";
  if (t == MsgVote) return "MsgVote";
  if (t == MsgVoteResp) return "MsgVoteResp";
  if (t == MsgSnap) return "MsgSnap";
  if (t == MsgHeartbeat) return "MsgHeartbeat";
  if (t == MsgHeartbeatResp) return "MsgHeartbeatResp";
  if (t == MsgUnreachable) return "MsgUnreachable";
  if (t == MsgSnapStatus) return "MsgSnapStatus";
  if (t == MsgCheckQuorum) return "MsgCheckQuorum";
  if (t == MsgTransferLeader) return "MsgTransferLeader";
  if (t == MsgTimeoutNow) return "MsgTimeoutNow";
  if (t == MsgReadIndex) return "MsgReadIndex";
  if (t == MsgReadIndexResp) return "MsgReadIndexResp";
  if (t == MsgPreVote) return "MsgPreVote";
  if (t == MsgPreVoteResp) return "MsgPreVoteResp";
  return "unknown msg";
}

string entryString(const Entry& entry) {
  char tmp[100];
  snprintf(tmp, sizeof(tmp), "term: %llu, index: %llu, data: %s", 
    entry.term(), entry.index(), entry.data().c_str());
  return tmp;
}

void copyEntries(const Message* msg, EntryVec *entries) {
  int i = 0;
  for (i = 0; i < msg->entries_size(); ++i) {
    entries->push_back(msg->entries(i));
  }
}

raft::raft(Config *config, raftLog *log)
  : id_(config->id),
    term_(0),
    vote_(0),
    raftLog_(log),
    maxInfilght_(config->maxInflightMsgs),
    maxMsgSize_(config->maxSizePerMsg),
    leader_(None),
    leadTransferee_(None),
    readOnly_(new readOnly(config->readOnlyOption, config->logger)),
    heartbeatTimeout_(config->heartbeatTick),
    electionTimeout_(config->electionTick),
    preVote_(config->preVote),
    logger_(config->logger) {
  srand((unsigned)time(NULL));
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
    r->prs_[peers[i]] = new Progress(1, r->maxInfilght_, logger);
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
  for (iter = r->prs_.begin(); iter != r->prs_.end(); ++iter) {
    snprintf(tmp, sizeof(tmp), "%llu", iter->first);
    peerStrs.push_back(tmp);
  }
  string nodeStr = joinStrings(peerStrs, ",");

  r->logger_->Infof(__FILE__, __LINE__,
    "newRaft %llu [peers: [%s], term: %llu, commit: %llu, applied: %llu, lastindex: %llu, lastterm: %llu]",
    r->id_, nodeStr.c_str(), r->term_, rl->committed_, rl->applied_, rl->lastIndex(), rl->lastTerm());
  return r;
}

void raft::tick() {
  switch (state_) {
  case StateFollower:
  case StateCandidate:
  case StatePreCandidate:
    tickElection();
    break;
  case StateLeader:
    tickHeartbeat();
    break;
  }
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
  if (type == MsgVote || type == MsgPreVote) {
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
        pr->ins_.add(last);
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
void raft::bcastHeartbeat() {
  string ctx = readOnly_->lastPendingRequestCtx();
  bcastHeartbeatWithCtx(ctx);
}

void raft::bcastHeartbeatWithCtx(const string &ctx) {
  map<uint64_t, Progress*>::const_iterator iter = prs_.begin();
  for (;iter != prs_.end();++iter) {
    if (iter->first == id_) {
      continue;
    }
    sendHeartbeat(iter->first, ctx);
  }
}

template <typename T>  
struct reverseCompartor {  
  bool operator()(const T &x, const T &y)  {  
    return y > x;  
  }  
};

// maybeCommit attempts to advance the commit index. Returns true if
// the commit index changed (in which case the caller should call
// r.bcastAppend).
bool raft::maybeCommit() {
  map<uint64_t, Progress*>::const_iterator iter;
  vector<uint64_t> mis;
  for (iter = prs_.begin(); iter != prs_.end(); ++iter) {
    mis.push_back(iter->second->match_);
  }
  sort(mis.begin(), mis.end(), reverseCompartor<uint64_t>());
  return raftLog_->maybeCommit(mis[quorum() - 1], term_);
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
    prs_[id] = new Progress(raftLog_->lastIndex() + 1, maxInfilght_, logger_);
    if (id == id_) {
      pr = prs_[id];
      pr->match_ = raftLog_->lastIndex();
    }
  }
  pendingConf_ = false;
  delete readOnly_;
  readOnly_ = new readOnly(readOnly_->option_, logger_);
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
  // Regardless of maybeCommit's return, our caller will call bcastAppend.
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
  return electionElapsed_ >= randomizedElectionTimeout_;
}

void raft::resetRandomizedElectionTimeout() {
  randomizedElectionTimeout_ = electionTimeout_ + rand() % electionTimeout_;
}

void raft::becomeFollower(uint64_t term, uint64_t leader) {
  reset(term);
  leader_ = leader;
  state_ = StateFollower;
  logger_->Infof(__FILE__, __LINE__, "%x became follower at term %llu", id_, term_);
}

void raft::becomePreCandidate() {
  // TODO(xiangli) remove the panic when the raft implementation is stable
  if (state_ == StateLeader) {
    logger_->Fatalf(__FILE__, __LINE__, "invalid transition [leader -> pre-candidate]");
  }
  // Becoming a pre-candidate changes our step functions and state,
  // but doesn't change anything else. In particular it does not increase
  // r.Term or change r.Vote.
  state_ = StatePreCandidate;
  logger_->Infof(__FILE__, __LINE__, "%x became pre-candidate at term %llu", id_, term_);
}

void raft::becomeCandidate() {
  if (state_ == StateLeader) {
    logger_->Fatalf(__FILE__, __LINE__, "invalid transition [leader -> candidate]");
  }

  reset(term_ + 1);
  vote_ = id_;
  state_ = StateCandidate;
  logger_->Infof(__FILE__, __LINE__, "%x became candidate at term %llu", id_, term_);
}

void raft::becomeLeader() {
  if (state_ == StateFollower) {
    logger_->Fatalf(__FILE__, __LINE__, "invalid transition [follower -> leader]");
  }

  reset(term_);
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

const char* raft::getCampaignString(CampaignType t) {
  switch (t) {
  case campaignPreElection:
    return "campaignPreElection";
  case campaignElection:
    return "campaignElection";
  case campaignTransfer:
    return "campaignTransfer";
  }
  return "unknown campaign type";
}

void raft::campaign(CampaignType t) {
  uint64_t term;
  MessageType voteMsg;
  if (t == campaignPreElection) {
    becomePreCandidate();
    voteMsg = MsgPreVote;
    term = term_ + 1;
  } else {
    becomeCandidate();
    voteMsg = MsgVote;
    term = term_;
  }

  if (quorum() == poll(id_, voteRespMsgType(voteMsg), true)) {
    // We won the election after voting for ourselves (which must mean that
    // this is a single-node cluster). Advance to the next state.
    if (t == campaignPreElection) {
      campaign(campaignElection);
    } else {
      becomeLeader();
    }
  }

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
    msg->set_to(id);
    msg->set_type(voteMsg);
    msg->set_index(raftLog_->lastIndex());
    msg->set_logterm(raftLog_->lastTerm());
    msg->set_context(ctx);
    send(msg);
  }
}

int raft::poll(uint64_t id, MessageType t, bool v) {
  if (v) {
    logger_->Infof(__FILE__, __LINE__, "%x received %s from %x at term %llu", id_, msgTypeString(t), id, term_);
  } else {
    logger_->Infof(__FILE__, __LINE__, "%x received %s rejection from %x at term %llu", id_, msgTypeString(t), id, term_);
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

int raft::step(Message *msg) {
  // Handle the message term, which may result in our stepping down to a follower.
  uint64_t term = msg->term();
  int type = msg->type();
  uint64_t from = msg->from();

  if (term_ != 0 && term > term_) {
    uint64_t leader = from;
    if (type == MsgVote || type == MsgPreVote) {
      bool force = msg->context() == kCampaignTransfer;
      bool inLease = checkQuorum_ && leader_ != None && electionElapsed_ < electionTimeout_;
      if (!force && inLease) {
        // If a server receives a RequestVote request within the minimum election timeout
        // of hearing from a current leader, it does not update its term or grant its vote
        logger_->Infof(__FILE__, __LINE__, "%x [logterm: %llu, index: %llu, vote: %x] ignored %s from %x [logterm: %llu, index: %llu] at term %llu: lease is not expired (remaining ticks: %llu",
          id_, raftLog_->lastTerm(), raftLog_->lastIndex(), vote_, msgTypeString(type), from,
          msg->logterm(), msg->index(), term, electionTimeout_ - electionElapsed_);
        return OK;
      }
      leader = None;
    }
    if (type == MsgPreVote) {
      // Never change our term in response to a PreVote
    } else if (type == MsgPreVoteResp && !msg->reject()) {
      //TODO
    } else {
      logger_->Infof(__FILE__, __LINE__, "%x [term: %llu] received a %s message with higher term from %x [term: %llu]",
        id_, term_, msgTypeString(type), from, term);
      becomeFollower(term, leader);
    }
  } else if (term < term_) {
    if (checkQuorum_ && (type == MsgHeartbeat || type == MsgApp)) {
      //TODO
    } else {
      logger_->Infof(__FILE__, __LINE__, "%x [term: %llu] ignored a %s message with lower term from %x [term: %llu]",
        id_, term_, msgTypeString(type), from, term);
    }
  }

  EntryVec entries;
  int err;
  int n;
  Message *respMsg;

  switch (type) {
  case MsgHup:
    if (state_ != StateLeader) {
      err = raftLog_->slice(raftLog_->applied_ + 1, raftLog_->committed_ + 1, noLimit, &entries);
      if (!SUCCESS(err)) {
        logger_->Fatalf(__FILE__, __LINE__, "unexpected error getting unapplied entries (%s)", GetErrorString(err));
      }
      n = numOfPendingConf(entries);
      if (n != 0 && raftLog_->committed_ > raftLog_->applied_) {
        logger_->Warningf(__FILE__, __LINE__, "%x cannot campaign at term %llu since there are still %llu pending configuration changes to apply",
          id_, term_, n);
        return OK;
      }
      logger_->Infof(__FILE__, __LINE__, "%x is starting a new election at term %llu", id_, term_);
      if (preVote_) {
        campaign(campaignPreElection);
      } else {
        campaign(campaignElection);
      }
    } else {
      logger_->Debugf(__FILE__, __LINE__, "%x ignoring MsgHup because already leader", id_);
    }
    break;
  case MsgVote:
  case MsgPreVote:
    // The m.Term > r.Term clause is for MsgPreVote. For MsgVote m.Term should
    // always equal r.Term.
    if ((vote_ == None || term > term_ || vote_ == from) && raftLog_->isUpToDate(msg->index(), msg->logterm())) {
      logger_->Infof(__FILE__, __LINE__, "%x [logterm: %llu, index: %llu, vote: %x] cast %s for %x [logterm: %llu, index: %llu] at term %llu",
        id_, raftLog_->lastTerm(), raftLog_->lastIndex(), vote_, msgTypeString(type), from, msg->logterm(), msg->index(), term_);
      respMsg = new Message();
      respMsg->set_to(from);      
      respMsg->set_type(voteRespMsgType(type));
      send(respMsg);
      if (type == MsgVote) {
        electionElapsed_ = 0;
        vote_ = from;
      }
    } else {
      logger_->Infof(__FILE__, __LINE__,
        "%x [logterm: %llu, index: %llu, vote: %x] rejected %s from %x [logterm: %llu, index: %llu] at term %llu",
        id_, raftLog_->lastTerm(), raftLog_->lastIndex(), vote_, msgTypeString(type), from, msg->logterm(), msg->index(), term_);
      respMsg = new Message();
      respMsg->set_to(from);      
      respMsg->set_reject(true);      
      respMsg->set_type(voteRespMsgType(type));
      send(msg);
    }
    break;
  default:
    if (proxyMessage(msg)) {
      delete msg;
    }
    break;
  }

  return OK;
}

bool raft::proxyMessage(Message *msg) {
  if (state_ == StateFollower) {
    return stepFollower(msg);
  }
  if (state_ == StateLeader) {
    return stepLeader(msg);
  }

  return stepCandidate(msg);
}

bool raft::stepLeader(Message *msg) {
  int type = msg->type();
  size_t i;
  uint64_t term;
  int err;
  EntryVec entries;

  switch (type) {
  case MsgBeat:
    bcastHeartbeat();
    return true;
    break;
  case MsgCheckQuorum:
    // TODO
    break;
  case MsgProp:
    if (msg->entries_size() == 0) {
      logger_->Fatalf(__FILE__, __LINE__, "%x stepped empty MsgProp", id_);
    }
    if (prs_.find(id_) == prs_.end()) {
      // If we are not currently a member of the range (i.e. this node
      // was removed from the configuration while serving as leader),
      // drop any new proposals.
      return true;
    }
    if (leadTransferee_ != None) {
      // TODO
    }
    for (i = 0; i < msg->entries_size(); ++i) {
      Entry *entry = msg->mutable_entries(i);
      if (entry->type() != EntryConfChange) {
        continue;
      }
      if (pendingConf_) {
        logger_->Infof(__FILE__, __LINE__, 
          "propose conf %s ignored since pending unapplied configuration",
          entryString(*entry).c_str());
        Entry tmp;
        tmp.set_type(EntryNormal);
        entry->CopyFrom(tmp);
      }
      pendingConf_ = true;
    }
    copyEntries(msg, &entries);
    appendEntry(&entries);
    bcastAppend();
    return true;
    break;
  case MsgReadIndex:
    if (quorum() > 1) {
      err = raftLog_->term(raftLog_->committed_, &term);
      if (raftLog_->zeroTermOnErrCompacted(term, err) != term_) {
        // Reject read only request when this leader has not committed any log entry at its term.
        return true;
      }
      // thinking: use an interally defined context instead of the user given context.
      // We can express this in terms of the term and index instead of a user-supplied value.
      // This would allow multiple reads to piggyback on the same message.
      if (readOnly_->option_ == ReadOnlySafe) {
        readOnly_->addRequest(raftLog_->committed_, msg);
        bcastHeartbeatWithCtx(msg->entries(0).data());
        return false; 
      } else if (readOnly_->option_ == ReadOnlyLeaseBased) {
        // TODO
      }
    } else {
      readStates_.push_back(new ReadState(raftLog_->committed_, msg->entries(0).data())); 
    }
    return true;
    break;
  }

  // All other message types require a progress for m.From (pr).
  Progress *pr;
  uint64_t from = msg->from();
  uint64_t index = msg->index();
  bool oldPaused;
  vector<readIndexStatus*> rss;
  Message *req, *respMsg;
  map<uint64_t, Progress*>::iterator iter = prs_.find(from);
  if (iter == prs_.end()) {
    logger_->Debugf(__FILE__, __LINE__, "%x no progress available for %x", id_, from);
    return true;
  }
  pr = iter->second;
  int ackCnt;

  switch (type) {
  case MsgAppResp:
    pr->recentActive_ = true;
    if (msg->reject()) {
      logger_->Debugf(__FILE__, __LINE__, "%x received msgApp rejection(lastindex: %llu) from %x for index %llu",
        id_, msg->rejecthint(), from, index);
      if (pr->maybeDecrTo(index, msg->rejecthint())) {
        logger_->Debugf(__FILE__, __LINE__, "%x decreased progress of %x to [%s]",
          id_, from, pr->string().c_str());
        if (pr->state_ == ProgressStateReplicate) {
          pr->becomeProbe();
        }
        sendAppend(from);
      }
    } else {
      oldPaused = pr->isPaused();
      if (pr->maybeUpdate(index)) {
        if (pr->state_ == ProgressStateProbe) {
          pr->becomeReplicate();
        } else if (pr->state_ == ProgressStateSnapshot && pr->needSnapshotAbort()) {
          logger_->Debugf(__FILE__, __LINE__, "%x snapshot aborted, resumed sending replication messages to %x [%s]",
            id_, from, pr->string().c_str());
          pr->becomeProbe();
        } else if (pr->state_ == ProgressStateReplicate) {
          pr->ins_.freeTo(index);
        }
      }
      if (maybeCommit()) {
        bcastAppend();
      } else if (oldPaused) {
        // update() reset the wait state on this node. If we had delayed sending
        // an update before, send it now.
        sendAppend(from);
      }
      // Transfer leadership is in progress.
      // TODO
    }
    break;
  case MsgHeartbeatResp:
    pr->recentActive_ = true;
    pr->resume();

    // free one slot for the full inflights window to allow progress.
    if (pr->state_ == ProgressStateReplicate && pr->ins_.full()) {
      pr->ins_.freeFirstOne();
    }
    if (pr->match_ < raftLog_->lastIndex()) {
      sendAppend(from);
    }

    if (readOnly_->option_ != ReadOnlySafe || msg->context().size() == 0) {
      return true;
    }

    ackCnt = readOnly_->recvAck(msg);
    if (ackCnt < quorum()) {
      return true;
    }
    readOnly_->advance(msg, &rss);
    for (i = 0; i < rss.size(); ++i) {
      req = rss[i]->req_;
      if (req->from() == None || req->from() == id_) {
        readStates_.push_back(new ReadState(rss[i]->index_, req->entries(0).data()));
      } else {
        respMsg = new Message();
        respMsg->set_type(MsgReadIndexResp); 
        respMsg->set_to(req->from()); 
        respMsg->set_index(rss[i]->index_);
        respMsg->mutable_entries()->CopyFrom(req->entries());
        send(respMsg);
      }
    } 
    break;
  case MsgSnapStatus:
    if (pr->state_ != ProgressStateSnapshot) {
      return true;
    }
    if (!msg->reject()) {
      pr->becomeProbe();
      logger_->Debugf(__FILE__, __LINE__, "%x snapshot succeeded, resumed sending replication messages to %x [%s]",
        id_, from, pr->string().c_str());
    } else {
      pr->snapshotFailure();
      pr->becomeProbe();
      logger_->Debugf(__FILE__, __LINE__, "%x snapshot failed, resumed sending replication messages to %x [%s]",
        id_, from, pr->string().c_str());
    }
    // If snapshot finish, wait for the msgAppResp from the remote node before sending
    // out the next msgApp.
    // If snapshot failure, wait for a heartbeat interval before next try
    pr->pause();
    break;
  case MsgUnreachable:
    //TODO
    break;
  case MsgTransferLeader:
    //TODO
    break;
  }
  
  return true;
}

// stepCandidate is shared by StateCandidate and StatePreCandidate; the difference is
// whether they respond to MsgVoteResp or MsgPreVoteResp.
bool raft::stepCandidate(Message *msg) {
  // Only handle vote responses corresponding to our candidacy (while in
  // StateCandidate, we may get stale MsgPreVoteResp messages in this term from
  // our pre-candidate state).
  MessageType voteRespType;
  if (state_ == StatePreCandidate) {
    voteRespType = MsgPreVoteResp;
  } else {
    voteRespType = MsgVoteResp;
  }
  int type = msg->type();
  int granted;

  if (type == voteRespType) {
    granted = poll(msg->from(), msg->type(), !msg->reject());
    logger_->Infof(__FILE__, __LINE__, "%x [quorum:%llu] has received %llu %s votes and %llu vote rejections",
      id_, quorum(), granted, msgTypeString(type), votes_.size() - granted);
    if (granted == quorum()) {
      if (state_ == StatePreCandidate) {
        // TODO
      } else {
        becomeLeader();
        bcastAppend();
      }
    } else if (granted == votes_.size() - granted) {
      becomeFollower(term_, None);
    }
    return true;
  }

  switch (type) {
  case MsgProp:
    logger_->Infof(__FILE__, __LINE__, "%x no leader at term %llu; dropping proposal", id_, term_);
    return true;
    break;
  case MsgApp:
    becomeFollower(term_, msg->from());
    handleAppendEntries(msg);
    break;
  case MsgHeartbeat:
    becomeFollower(term_, msg->from());
    handleHeartbeat(msg);
    break;
  case MsgTimeoutNow:
    // TODO
    break;
  }

  return true;
}

bool raft::stepFollower(Message *msg) {
  int type = msg->type();

  switch (type) {
  case MsgProp:
    if (leader_ == None) {
      return true;
    }
    msg->set_to(leader_);
    send(msg);
    return false;
    break;
  case MsgApp:
    electionElapsed_ = 0;
    leader_ = msg->from();
    handleAppendEntries(msg);
    break;
  case MsgHeartbeat:
    electionElapsed_ = 0;
    leader_ = msg->from();
    handleHeartbeat(msg);
    break;
  case MsgSnap:
    electionElapsed_ = 0;
    leader_ = msg->from();
    handleSnapshot(msg);
    break;
  case MsgTransferLeader:
    // TODO
    break;
  case MsgTimeoutNow:
    // TODO
    break;
  case MsgReadIndex:
    if (leader_ == None) {
      logger_->Infof(__FILE__, __LINE__, "%x no leader at term %llu; dropping index reading msg", id_, term_);
      return true;
    }   
    msg->set_to(leader_);
    send(msg);
    return false;
    break;
  case MsgReadIndexResp:
    if (msg->entries_size() != 1) {
      logger_->Errorf(__FILE__, __LINE__, "%x invalid format of MsgReadIndexResp from %x, entries count: %llu",
        id_, msg->from(), msg->entries_size());
      return true;
    }
    readStates_.push_back(new ReadState(msg->index(), msg->entries(0).data()));
    break;
  }

  return true;
}

// restore recovers the state machine from a snapshot. It restores the log and the
// configuration of state machine.
bool raft::restore(const Snapshot& snapshot) {
  if (snapshot.metadata().index() <= raftLog_->committed_) {
    return false;
  }

  if (raftLog_->matchTerm(snapshot.metadata().index(), snapshot.metadata().term())) {
    logger_->Infof(__FILE__, __LINE__, "%x [commit: %llu, lastindex: %llu, lastterm: %llu] fast-forwarded commit to snapshot [index: %llu, term: %llu]",
      id_, raftLog_->committed_, raftLog_->lastIndex(), raftLog_->lastTerm(),
       snapshot.metadata().index(), snapshot.metadata().term());
    raftLog_->commitTo(snapshot.metadata().index());
    return false;
  }
  logger_->Infof(__FILE__, __LINE__, "%x [commit: %llu, lastindex: %llu, lastterm: %llu] starts to restore snapshot [index: %llu, term: %llu]",
    id_, raftLog_->committed_, raftLog_->lastIndex(), raftLog_->lastTerm(),
    snapshot.metadata().index(), snapshot.metadata().term());
  raftLog_->restore(snapshot);
  prs_.clear();
  int i;
  for (i = 0; i < snapshot.metadata().conf_state().nodes_size(); ++i) {
    uint64_t node = snapshot.metadata().conf_state().nodes(i);
    uint64_t match = 0;
    uint64_t next = raftLog_->lastIndex() + 1; 
    if (node == id_) {
      match = next - 1;
    }
    setProgress(node, match, next);
    logger_->Infof(__FILE__, __LINE__, "%x restored progress of %x [%s]", id_, node, prs_[node]->string().c_str());
  }
  return true;
}

void raft::handleSnapshot(Message *msg) {
  uint64_t sindex = msg->snapshot().metadata().index();
  uint64_t sterm  = msg->snapshot().metadata().term();
  Message *resp = new Message;

  resp->set_to(msg->from());
  resp->set_type(MsgAppResp);
  if (restore(msg->snapshot())) {
    logger_->Infof(__FILE__, __LINE__, "%x [commit: %d] restored snapshot [index: %d, term: %d]",
      id_, raftLog_->committed_, sindex, sterm);
    resp->set_index(raftLog_->lastIndex());
  } else {
    logger_->Infof(__FILE__, __LINE__, "%x [commit: %d] ignored snapshot [index: %d, term: %d]",
      id_, raftLog_->committed_, sindex, sterm);
    resp->set_index(raftLog_->committed_);
  }
  send(resp);
}

void raft::handleHeartbeat(Message *msg) {
  raftLog_->commitTo(msg->commit());
  Message *resp = new Message();
  resp->set_to(msg->from());
  resp->set_type(MsgHeartbeatResp);
  resp->set_context(msg->context());
  send(resp);
}

void raft::handleAppendEntries(Message *msg) {
  if (msg->index() < raftLog_->committed_) {
    Message *resp = new Message();
    resp->set_to(msg->from());
    resp->set_type(MsgAppResp);
    resp->set_index(raftLog_->committed_);
    send(resp);
    return;
  }

  EntryVec entries;
  copyEntries(msg, &entries);
  uint64_t lasti;
  bool ret = raftLog_->maybeAppend(msg->index(), msg->logterm(), msg->commit(), entries, &lasti);
  if (ret) {
    Message *resp = new Message();
    resp->set_to(msg->from());
    resp->set_type(MsgAppResp);
    resp->set_index(lasti);
    send(resp);
  } else {
    uint64_t term;
    int err = raftLog_->term(msg->index(), &term);
    logger_->Debugf(__FILE__, __LINE__, "%x [logterm: %llu, index: %llu] rejected msgApp [logterm: %llu, index: %llu] from %x",
      id_, raftLog_->zeroTermOnErrCompacted(term, err), msg->index(), msg->logterm(), msg->index(), msg->from());
    Message *resp = new Message();
    resp->set_to(msg->from());
    resp->set_type(MsgAppResp);
    resp->set_index(msg->index());
    resp->set_reject(true);
    resp->set_rejecthint(raftLog_->lastIndex());
    send(resp);
  }
}

void raft::setProgress(uint64_t id, uint64_t match, uint64_t next) {
  if (prs_[id] != NULL)  {
    delete prs_[id];
  }
  prs_[id] = new Progress(next, maxInfilght_, logger_);
  prs_[id]->match_ = match;
}

void raft::delProgress(uint64_t id) {
  delete prs_[id];
  prs_.erase(id);
}

void raft::abortLeaderTransfer() {
  leadTransferee_ = None;
}

void raft::addNode(uint64_t id) {
  pendingConf_ = false;
  if (prs_.find(id) != prs_.end()) {
    return;
  }
  setProgress(id, 0, raftLog_->lastIndex() + 1);
}

void raft::removeNode(uint64_t id) {
  delProgress(id);
  pendingConf_ = false;

  // do not try to commit or abort transferring if there is no nodes in the cluster.
  if (prs_.empty()) {
    return;
  }

  // The quorum size is now smaller, so see if any pending entries can
  // be committed.
  if (maybeCommit()) {
    bcastAppend();
  }

  // If the removed node is the leadTransferee, then abort the leadership transferring.
  if (state_ == StateLeader && leadTransferee_ == id) {
    abortLeaderTransfer();
  }
}

void raft::readMessages(vector<Message*> *msgs) {
  *msgs = msgs_;
  msgs_.clear();
}
