/*
 * Copyright (C) lichuang
 */

#include <stdlib.h>
#include <time.h>
#include <algorithm>
#include "base/util.h"
#include "core/raft.h"
#include "core/read_only.h"

namespace libraft {

static const char* kCampaignString[] = {
  "CampaignPreElection",
  "CampaignElection",
  "CampaignTransfer",
};

static const char* kMsgString[] = {
  "MsgHup",
  "MsgBeat",
  "MsgProp",
  "MsgApp",
  "MsgAppResp",
  "MsgVote",
  "MsgVoteResp",
  "MsgSnap",
  "MsgHeartbeat",
  "MsgHeartbeatResp",
  "MsgUnreachable",
  "MsgSnapStatus",
  "MsgCheckQuorum",
  "MsgTransferLeader",
  "MsgTimeoutNow",
  "MsgReadIndex",
  "MsgReadIndexResp",
  "MsgPreVote",
  "MsgPreVoteResp",
};

string 
entryString(const Entry& entry) {
  char tmp[100];
  snprintf(tmp, sizeof(tmp), "term:%llu, index:%llu, type:%d", 
    entry.term(), entry.index(), entry.type());
  string str = tmp;
  str += ", data:" + entry.data() + "\n";
  return str;
}

static void 
copyEntries(const Message& msg, EntryVec *entries) {
  int i = 0;
  for (i = 0; i < msg.entries_size(); ++i) {
    entries->push_back(msg.entries(i));
  }
}

static Message*
cloneMessage(const Message& msg) {
  return new Message(msg);
}

raft::raft(const Config *config, raftLog *log)
  : id_(config->id),
    term_(0),
    vote_(0),
    raftLog_(log),
    maxInfilght_(config->maxInflightMsgs),
    maxMsgSize_(config->maxSizePerMsg),
    leader_(kNone),
    leadTransferee_(kNone),
    readOnly_(new readOnly(config->readOnlyOption, config->logger)),
    heartbeatTimeout_(config->heartbeatTick),
    electionTimeout_(config->electionTick),
    checkQuorum_(config->checkQuorum),
    preVote_(config->preVote),
    logger_(config->logger),
    stateStepFunc_(NULL) {
  srand((unsigned)time(NULL));
}

void
raft::tick() {
  switch (state_) {
  case StateFollower:
  case StateCandidate:
  case StatePreCandidate:
    tickElection();
    break;
  case StateLeader:
    tickHeartbeat();
    break;
  default:
    logger_->Fatalf(__FILE__, __LINE__, "unsport state %d", state_);
    break;
  }
}

// checkQuorumActive returns true if the quorum is active from
// the view of the local raft state machine. Otherwise, it returns
// false.
// checkQuorumActive also resets all RecentActive to false.
bool 
raft::checkQuorumActive() {
  int act = 0;

  map<uint64_t, Progress*>::const_iterator iter;
  for (iter = progressMap_.begin(); iter != progressMap_.end(); ++iter) {
    if (iter->first == id_) { // self is always active
      act++;
      continue;
    }
    if (!iter->second->recentActive_) {
      continue;
    }
    act++;
    iter->second->recentActive_ = false;     
  }

  return act >= quorum();
}

bool
raft::hasLeader() {
  return leader_ != kNone;
}

void
raft::softState(SoftState *ss) {
  ss->leader = leader_;
  ss->state  = state_;
}

void
raft::hardState(HardState *hs) {
  hs->set_term(term_);
  hs->set_vote(vote_);
  hs->set_commit(raftLog_->committed_);
}

void
raft::nodes(vector<uint64_t> *nodes) {
  nodes->clear();
  map<uint64_t, Progress*>::const_iterator iter = progressMap_.begin();
  while (iter != progressMap_.end()) {
    nodes->push_back(iter->first);
    ++iter;
  }
}

void
raft::loadState(const HardState &hs) {
  // validity check
  if (hs.commit() < raftLog_->committed_ || hs.commit() > raftLog_->lastIndex()) {
    logger_->Fatalf(__FILE__, __LINE__, 
      "%x state.commit %llu is out of range [%llu, %llu]", id_, hs.commit(), raftLog_->committed_, raftLog_->lastIndex());
  }

  raftLog_->committed_ = hs.commit();
  term_ = hs.term();
  vote_ = hs.vote();
}

int
raft::quorum() {
  return (progressMap_.size() / 2) + 1;
}

// send persists state to stable storage and then sends to its mailbox.
void
raft::send(Message *msg) {
  msg->set_from(id_);
  int type = msg->type();

  if (type == MsgVote || type == MsgPreVote) {
    if (msg->term() == 0) {
      // PreVote RPCs are sent at a term other than our actual term, so the code
      // that sends these messages is responsible for setting the term.
      logger_->Fatalf(__FILE__, __LINE__, "term should be set when sending %s", kMsgString[type]);
    }
  } else {
    if (msg->term() != 0) { 
      logger_->Fatalf(__FILE__, __LINE__, "term should not be set when sending %s (was %llu)", kMsgString[type], msg->term());
    }
    // do not attach term to MsgProp, MsgReadIndex
    // proposals are a way to forward to the leader and
    // should be treated as local message.
    // MsgReadIndex is also forwarded to leader.
    if (type != MsgProp && type != MsgReadIndex) {
      msg->set_term(term_);
    }
  }

  // save every out msg in outMsgs_,then msgs will be moved to `Ready' struct
  outMsgs_.push_back(msg);
}

// sendAppend sends RPC, with entries to the given peer.
void
raft::sendAppend(uint64_t to) {
  Progress *pr = progressMap_[to];
  if (pr == NULL || pr->isPaused()) {
    logger_->Infof(__FILE__, __LINE__, "node %x paused", to);
    return;
  }

  Message *msg = new Message();
  msg->set_to(to);

  uint64_t term;
  int errt, erre, err;
  EntryVec entries;
  Snapshot *snapshot;

  // try to get term and entities
  errt = raftLog_->term(pr->next_ - 1, &term);
  erre = raftLog_->entries(pr->next_, maxMsgSize_, &entries);
  if (!SUCCESS(errt) || !SUCCESS(erre)) {
    // send snapshot if we failed to get term or entries
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
      id_, raftLog_->firstIndex(), raftLog_->committed_, sindex, sterm, to, pr->String().c_str());
    // change to snapshot state
    pr->becomeSnapshot(sindex);
    logger_->Debugf(__FILE__, __LINE__, "%x paused sending replication messages to %x [%s]", id_, to, pr->String().c_str());
  } else {
    // else,send Append Msg
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
        pr->inflights_.add(last);
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
void
raft::sendHeartbeat(uint64_t to, const string &ctx) {
  // Attach the commit as min(to.matched, r.committed).
  // When the leader sends out heartbeat message,
  // the receiver(follower) might not be matched with the leader
  // or it might not have all the committed entries.
  // The leader MUST NOT forward the follower's commit to
  // an unmatched index.
  uint64_t commit = min(progressMap_[to]->match_, raftLog_->committed_);
  Message *msg = new Message();
  msg->set_to(to);
  msg->set_type(MsgHeartbeat);
  msg->set_commit(commit);
  msg->set_context(ctx);
  send(msg);
}

// bcastAppend sends RPC, with entries to all peers that are not up-to-date
// according to the progress recorded in r.prs.
void
raft::bcastAppend() {
  map<uint64_t, Progress*>::const_iterator iter = progressMap_.begin();
  for (;iter != progressMap_.end();++iter) {
    if (iter->first == id_) {
      continue;
    }
    sendAppend(iter->first);
  }
}

// bcastHeartbeat sends RPC, without entries to all the peers.
void
raft::bcastHeartbeat() {
  string ctx = readOnly_->lastPendingRequestCtx();
  bcastHeartbeatWithCtx(ctx);
}

void
raft::bcastHeartbeatWithCtx(const string &ctx) {
  map<uint64_t, Progress*>::const_iterator iter = progressMap_.begin();
  for (;iter != progressMap_.end();++iter) {
    if (iter->first == id_) {
      continue;
    }
    sendHeartbeat(iter->first, ctx);
  }
}

template <typename T>
struct reverseCompartor {  
  bool operator()(const T &x, const T &y)  {
    return y < x;
  }  
};

// maybeCommit attempts to advance the commit index. Returns true if
// the commit index changed (in which case the caller should 
// call r.bcastAppend).
bool
raft::maybeCommit() {
  map<uint64_t, Progress*>::const_iterator iter;
  vector<uint64_t> mis;
  for (iter = progressMap_.begin(); iter != progressMap_.end(); ++iter) {
    mis.push_back(iter->second->match_);
  }
  sort(mis.begin(), mis.end(), reverseCompartor<uint64_t>());
  return raftLog_->maybeCommit(mis[quorum() - 1], term_);
}

void
raft::reset(uint64_t term) {
  if (term_ != term) {
    term_ = term;
    vote_ = kNone;
  }
  leader_ = kNone;

  electionElapsed_ = 0;
  heartbeatElapsed_ = 0;
  resetRandomizedElectionTimeout();

  abortLeaderTransfer();
  votes_.clear();
  map<uint64_t, Progress*>::iterator iter = progressMap_.begin();
  for (; iter != progressMap_.end(); ++iter) {
    uint64_t id = iter->first;
    Progress *pr = progressMap_[id];
    delete pr;
    progressMap_[id] = new Progress(raftLog_->lastIndex() + 1, maxInfilght_, logger_);
    if (id == id_) {
      pr = progressMap_[id];
      pr->match_ = raftLog_->lastIndex();
    }
  }
  pendingConf_ = false;
  delete readOnly_;
  readOnly_ = new readOnly(readOnly_->option_, logger_);
}

void
raft::appendEntry(EntryVec* entries) {
  uint64_t li = raftLog_->lastIndex();
  logger_->Debugf(__FILE__, __LINE__, "lastIndex:%llu", li);
  size_t i;
  for (i = 0; i < entries->size(); ++i) {
    (*entries)[i].set_term(term_);
    (*entries)[i].set_index(li + 1 + i);
  }
  raftLog_->append(*entries);
  progressMap_[id_]->maybeUpdate(raftLog_->lastIndex());
  // Regardless of maybeCommit's return, our caller will call bcastAppend.
  maybeCommit();
}

// tickElection is run by followers and candidates after r.electionTimeout.
void
raft::tickElection() {
  electionElapsed_++;
  
  if (promotable() && pastElectionTimeout()) {
    electionElapsed_ = 0;
    Message msg;
    msg.set_from(id_);
    msg.set_type(MsgHup);
    step(msg);
  }
}

// tickHeartbeat is run by leaders to send a MsgBeat after r.heartbeatTimeout.
void
raft::tickHeartbeat() {
  heartbeatElapsed_++;
  electionElapsed_++;

  if (electionElapsed_ >= electionTimeout_) {
    electionElapsed_ = 0;
    if (checkQuorum_) { // send MsgCheckQuorum
      Message msg;
      msg.set_from(id_);
      msg.set_type(MsgCheckQuorum);
      step(msg);
    }
    // If current leader cannot transfer leadership in electionTimeout, it becomes leader again.
    if (state_ == StateLeader && leadTransferee_ != kNone) {
      abortLeaderTransfer();
    }
  }

  if (state_ != StateLeader) {
    return;
  }

  // if heartbeat timeout,send MsgBeat
  if (heartbeatElapsed_ >= heartbeatTimeout_) {
    heartbeatElapsed_ = 0;

    Message msg;
    msg.set_from(id_);
    msg.set_type(MsgBeat);
    step(msg);
  }
}

// promotable indicates whether state machine can be promoted to leader,
// which is true when its own id is in progress list.
bool
raft::promotable() {
  return progressMap_.find(id_) != progressMap_.end();
}

// pastElectionTimeout returns true if r.electionElapsed is greater
// than or equal to the randomized election timeout in
// [electiontimeout, 2 * electiontimeout - 1].
bool
raft::pastElectionTimeout() {
  return electionElapsed_ >= randomizedElectionTimeout_;
}

void
raft::resetRandomizedElectionTimeout() {
  randomizedElectionTimeout_ = electionTimeout_ + rand() % electionTimeout_;
}

void
raft::becomeFollower(uint64_t term, uint64_t leader) {
  reset(term);
  leader_ = leader;
  state_ = StateFollower;
  stateStepFunc_ = stepFollower;
  logger_->Infof(__FILE__, __LINE__, "%x became follower at term %llu", id_, term_);
}

void
raft::becomePreCandidate() {
  // TODO(xiangli) remove the panic when the raft implementation is stable
  if (state_ == StateLeader) {
    logger_->Fatalf(__FILE__, __LINE__, "invalid transition [leader -> pre-candidate]");
  }
  // Becoming a pre-candidate changes our step functions and state,
  // but doesn't change anything else. In particular it does not increase
  // r.Term or change r.Vote.
  state_ = StatePreCandidate;
  stateStepFunc_ = stepCandidate;
  logger_->Infof(__FILE__, __LINE__, "%x became pre-candidate at term %llu", id_, term_);
}

void
raft::becomeCandidate() {
  if (state_ == StateLeader) {
    logger_->Fatalf(__FILE__, __LINE__, "invalid transition [leader -> candidate]");
  }

  // add term + 1 in candidate state
  reset(term_ + 1);
  vote_ = id_;
  state_ = StateCandidate;
  stateStepFunc_ = stepCandidate;
  logger_->Infof(__FILE__, __LINE__, "%x became candidate at term %llu", id_, term_);
}

void
raft::becomeLeader() {
  if (state_ == StateFollower) {
    logger_->Fatalf(__FILE__, __LINE__, "invalid transition [follower -> leader]");
  }

  reset(term_);
  leader_ = id_;
  state_ = StateLeader;
  stateStepFunc_ = stepLeader;

  EntryVec entries;
  int err = raftLog_->entries(raftLog_->committed_ + 1, kNoLimit, &entries);
  // fatal if leader get committed entries fail
  if (!SUCCESS(err)) {
    logger_->Fatalf(__FILE__, __LINE__, "unexpected error getting uncommitted entries (%s)", GetErrorString(err));
  }

  int n = numOfPendingConf(entries);
  // fatal if leader has pending config > 1
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

void
raft::campaign(CampaignType t) {
  uint64_t term;
  MessageType voteMsg;

  if (t == CampaignPreElection) {
    becomePreCandidate();
    voteMsg = MsgPreVote;
    // PreVote RPCs are sent for the next term before we've incremented r.Term.
    term = term_ + 1;
  } else {
    becomeCandidate();
    voteMsg = MsgVote;
    term = term_;
  }

  if (quorum() == poll(id_, voteRespMsgType(voteMsg), true)) {
    // We won the election after voting for ourselves (which must mean that
    // this is a single-node cluster). Advance to the next state.
    if (t == CampaignPreElection) {
      campaign(CampaignElection);
    } else {
      becomeLeader();
    }
  }

  map<uint64_t, Progress*>::const_iterator iter = progressMap_.begin();
  for (; iter != progressMap_.end(); ++iter) {
    uint64_t id = iter->first;
    if (id_ == id) {
      continue;
    }
    logger_->Infof(__FILE__, __LINE__, "%x [logterm: %llu, index: %llu] sent %s request to %x at term %llu",
      id_, raftLog_->lastTerm(), raftLog_->lastIndex(), kCampaignString[t], id, term_);
    string ctx = "";
    if (t == CampaignTransfer) {
      ctx = kCampaignString[CampaignTransfer];
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

int
raft::poll(uint64_t id, MessageType t, bool v) {
  if (v) {
    logger_->Infof(__FILE__, __LINE__, "%x received %s from %x at term %llu", id_, kMsgString[t], id, term_);
  } else {
    logger_->Infof(__FILE__, __LINE__, "%x received %s rejection from %x at term %llu", id_, kMsgString[t], id, term_);
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

int
raft::step(const Message& msg) {
  logger_->Debugf(__FILE__, __LINE__, "msg %s %llu -> %llu, term:%llu",
                  kMsgString[msg.type()], msg.from(), msg.to(), term_);

  // Handle the message term, which may result in our stepping down to a follower.
  Message *respMsg;
  uint64_t term = msg.term();
  int type = msg.type();
  uint64_t from = msg.from();

  if (term == 0) {
    // ignore term 0 message
  } else if (term > term_) {
    uint64_t leader = from;
    if (type == MsgVote || type == MsgPreVote) {
      bool force = (msg.context() == kCampaignString[CampaignTransfer]);
      bool inLease = (checkQuorum_ && leader_ != kNone && electionElapsed_ < electionTimeout_);
      if (!force && inLease) {
        // If a server receives a RequestVote request within the minimum election timeout
        // of hearing from a current leader, it does not update its term or grant its vote
        logger_->Infof(__FILE__, __LINE__, "%x [logterm: %llu, index: %llu, vote: %x] ignored %s from %x [logterm: %llu, index: %llu] at term %llu: lease is not expired (remaining ticks: %d)",
          id_, raftLog_->lastTerm(), raftLog_->lastIndex(), vote_, kMsgString[type], from,
          msg.logterm(), msg.index(), term, electionTimeout_ - electionElapsed_);
        return OK;
      }
      leader = kNone;
    }
    if (type == MsgPreVote) {
      // Never change our term in response to a PreVote
    } else if (type == MsgPreVoteResp && !msg.reject()) {
      // We send pre-vote requests with a term in our future. If the
      // pre-vote is granted, we will increment our term when we get a
      // quorum. If it is not, the term comes from the node that
      // rejected our vote so we should become a follower at the new
      // term.
    } else {
      logger_->Infof(__FILE__, __LINE__, "%x [term: %llu] received a %s message with higher term from %x [term: %llu]",
        id_, term_, kMsgString[type], from, term);
      becomeFollower(term, leader);
    }
  } else if (term < term_) {
    if (checkQuorum_ && (type == MsgHeartbeat || type == MsgApp)) {
      // We have received messages from a leader at a lower term. It is possible
      // that these messages were simply delayed in the network, but this could
      // also mean that this node has advanced its term number during a network
      // partition, and it is now unable to either win an election or to rejoin
      // the majority on the old term. If checkQuorum is false, this will be
      // handled by incrementing term numbers in response to MsgVote with a
      // higher term, but if checkQuorum is true we may not advance the term on
      // MsgVote and must generate other messages to advance the term. The net
      // result of these two features is to minimize the disruption caused by
      // nodes that have been removed from the cluster's configuration: a
      // removed node will send MsgVotes (or MsgPreVotes) which will be ignored,
      // but it will not receive MsgApp or MsgHeartbeat, so it will not create
      // disruptive term increases
      respMsg = new Message();
      respMsg->set_to(from);
      respMsg->set_type(MsgAppResp);
      send(respMsg);
    } else {
      // ignore other cases
      logger_->Infof(__FILE__, __LINE__, "%x [term: %llu] ignored a %s message with lower term from %x [term: %llu]",
        id_, term_, kMsgString[type], from, term);
    }
    return OK;
  }

  EntryVec entries;
  int err;
  int n;

  switch (type) {
  case MsgHup:
    if (state_ != StateLeader) {
      err = raftLog_->slice(raftLog_->applied_ + 1, raftLog_->committed_ + 1, kNoLimit, &entries);
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
        campaign(CampaignPreElection);
      } else {
        campaign(CampaignElection);
      }
    } else {
      logger_->Debugf(__FILE__, __LINE__, "%x ignoring MsgHup because already leader", id_);
    }
    break;
  case MsgVote:
  case MsgPreVote:
    // The m.Term > r.Term clause is for MsgPreVote. For MsgVote m.Term should
    // always equal r.Term.
    if ((vote_ == kNone || term > term_ || vote_ == from) && raftLog_->isUpToDate(msg.index(), msg.logterm())) {
      logger_->Infof(__FILE__, __LINE__, "%x [logterm: %llu, index: %llu, vote: %x] cast %s for %x [logterm: %llu, index: %llu] at term %llu",
        id_, raftLog_->lastTerm(), raftLog_->lastIndex(), vote_, kMsgString[type], from, msg.logterm(), msg.index(), term_);
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
        id_, raftLog_->lastTerm(), raftLog_->lastIndex(), vote_, kMsgString[type], from, msg.logterm(), msg.index(), term_);
      respMsg = new Message();
      respMsg->set_to(from);      
      respMsg->set_reject(true);      
      respMsg->set_type(voteRespMsgType(type));
      send(respMsg);
    }
    break;
  default:
    // handle msg in different state function
    stateStepFunc_(this, msg);
    break;
  }

  return OK;
}

void 
stepLeader(raft *r, const Message& msg) {
  int type = msg.type();
  size_t i;
  uint64_t term, ri;
  int err;
  uint64_t lastLeadTransferee, leadTransferee;
  EntryVec entries;
  Message *n;
  Logger* logger = r->logger_;

  switch (type) {
  case MsgBeat:
    r->bcastHeartbeat();
    return;
    break;
  case MsgCheckQuorum:
    if (!r->checkQuorumActive()) {
      logger->Warningf(__FILE__, __LINE__, "%x stepped down to follower since quorum is not active", r->id_);
      r->becomeFollower(r->term_, kNone);
    }
    return;
    break;
  case MsgProp:
    if (msg.entries_size() == 0) {  // received a empty entries MsgProp
      logger->Fatalf(__FILE__, __LINE__, "%x stepped empty MsgProp", r->id_);
    }
    if (r->progressMap_.find(r->id_) == r->progressMap_.end()) {
      // If we are not currently a member of the range (i.e. this node
      // was removed from the configuration while serving as leader),
      // drop any new proposals.
      return;
    }
    if (r->leadTransferee_ != kNone) {
      logger->Debugf(__FILE__, __LINE__,
        "%x [term %d] transfer leadership to %x is in progress; dropping proposal",
        r->id_, r->term_, r->leadTransferee_);
      return;
    }
    n = cloneMessage(msg);
    for (i = 0; (int)i < n->entries_size(); ++i) {
      Entry *entry = n->mutable_entries(i);
      if (entry->type() != EntryConfChange) {
        continue;
      }
      if (r->pendingConf_) {
        logger->Infof(__FILE__, __LINE__, 
          "propose conf %s ignored since pending unapplied configuration",
          entryString(*entry).c_str());
        Entry tmp;
        tmp.set_type(EntryNormal);
        entry->CopyFrom(tmp);
      }
      r->pendingConf_ = true;
    }
    copyEntries(*n, &entries);
    r->appendEntry(&entries);
    r->bcastAppend();
    delete n;
    return;
    break;
  case MsgReadIndex:
    if (r->quorum() > 1) {
      err = r->raftLog_->term(r->raftLog_->committed_, &term);
      if (r->raftLog_->zeroTermOnErrCompacted(term, err) != r->term_) {
        // Reject read only request when this leader has not committed any log entry at its term.
        return;
      }
      // thinking: use an interally defined context instead of the user given context.
      // We can express this in terms of the term and index instead of a user-supplied value.
      // This would allow multiple reads to piggyback on the same message.
      if (r->readOnly_->option_ == ReadOnlySafe) {
        n = cloneMessage(msg);
        r->readOnly_->addRequest(r->raftLog_->committed_, n);
        r->bcastHeartbeatWithCtx(n->entries(0).data());
        return;
      } else if (r->readOnly_->option_ == ReadOnlyLeaseBased) {
        ri = 0;
        if (r->checkQuorum_) {
          ri = r->raftLog_->committed_;
        }
        if (msg.from() == kNone || msg.from() == r->id_) { // from local member
          r->readStates_.push_back(new ReadState(r->raftLog_->committed_, msg.entries(0).data())); 
        } else {
          n = cloneMessage(msg);
          n->set_to(msg.from());
          n->set_type(MsgReadIndexResp);
          n->set_index(ri);
          r->send(n);
        }
      }
    } else {
     r->readStates_.push_back(new ReadState(r->raftLog_->committed_, msg.entries(0).data())); 
    }
    return;
    break;
  }

  // All other message types require a progress for m.From (pr).
  Progress *pr;
  uint64_t from = msg.from();
  uint64_t index = msg.index();
  bool oldPaused;
  vector<readIndexStatus*> rss;
  Message *req, *respMsg;
  map<uint64_t, Progress*>::iterator iter = r->progressMap_.find(from);
  if (iter == r->progressMap_.end()) {
    logger->Debugf(__FILE__, __LINE__, "%x no progress available for %x", r->id_, from);
    return;
  }
  pr = iter->second;
  int ackCnt;

  switch (type) {
  case MsgAppResp:
    pr->recentActive_ = true;
    if (msg.reject()) {
      logger->Debugf(__FILE__, __LINE__, "%x received msgApp rejection(lastindex: %llu) from %x for index %llu",
        r->id_, msg.rejecthint(), from, index);
      if (pr->maybeDecrTo(index, msg.rejecthint())) {
        logger->Debugf(__FILE__, __LINE__, "%x decreased progress of %x to [%s]",
          r->id_, from, pr->String().c_str());
        if (pr->state_ == ProgressStateReplicate) {
          pr->becomeProbe();
        }
        r->sendAppend(from);
      }
    } else {
      oldPaused = pr->isPaused();
      if (pr->maybeUpdate(index)) {
        if (pr->state_ == ProgressStateProbe) {
          pr->becomeReplicate();
        } else if (pr->state_ == ProgressStateSnapshot && pr->needSnapshotAbort()) {
          logger->Debugf(__FILE__, __LINE__, "%x snapshot aborted, resumed sending replication messages to %x [%s]",
            r->id_, from, pr->String().c_str());
          pr->becomeProbe();
        } else if (pr->state_ == ProgressStateReplicate) {
          pr->inflights_.freeTo(index);
        }
        if (r->maybeCommit()) {
          r->bcastAppend();
        } else if (oldPaused) {
          // update() reset the wait state on this node. If we had delayed sending
          // an update before, send it now.
          r->sendAppend(from);
        }
        // Transfer leadership is in progress.
        if (msg.from() == r->leadTransferee_ && pr->match_ == r->raftLog_->lastIndex()) {
          logger->Infof(__FILE__, __LINE__,
            "%x sent MsgTimeoutNow to %x after received MsgAppResp", r->id_, msg.from());
          r->sendTimeoutNow(msg.from());
        }
      }
    }
    break;
  case MsgHeartbeatResp:
    pr->recentActive_ = true;
    pr->resume();

    // free one slot for the full inflights window to allow progress.
    if (pr->state_ == ProgressStateReplicate && pr->inflights_.full()) {
      pr->inflights_.freeFirstOne();
    }
    if (pr->match_ < r->raftLog_->lastIndex()) {
      r->sendAppend(from);
    }

    if (r->readOnly_->option_ != ReadOnlySafe || msg.context().empty()) {
      return;
    }

    ackCnt = r->readOnly_->recvAck(msg);
    if (ackCnt < r->quorum()) {
      return;
    }
    r->readOnly_->advance(msg, &rss);
    for (i = 0; i < rss.size(); ++i) {
      req = rss[i]->req_;
      if (req->from() == kNone || req->from() == r->id_) {
        r->readStates_.push_back(new ReadState(rss[i]->index_, req->entries(0).data()));
      } else {
        respMsg = new Message();
        respMsg->set_type(MsgReadIndexResp); 
        respMsg->set_to(req->from()); 
        respMsg->set_index(rss[i]->index_);
        respMsg->mutable_entries()->CopyFrom(req->entries());
        r->send(respMsg);
      }
    } 
    break;
  case MsgSnapStatus:
    if (pr->state_ != ProgressStateSnapshot) {
      return;
    }
    if (!msg.reject()) {
      pr->becomeProbe();
      logger->Debugf(__FILE__, __LINE__, "%x snapshot succeeded, resumed sending replication messages to %x [%s]",
        r->id_, from, pr->String().c_str());
    } else {
      pr->snapshotFailure();
      pr->becomeProbe();
      logger->Debugf(__FILE__, __LINE__, "%x snapshot failed, resumed sending replication messages to %x [%s]",
        r->id_, from, pr->String().c_str());
    }
    // If snapshot finish, wait for the msgAppResp from the remote node before sending
    // out the next msgApp.
    // If snapshot failure, wait for a heartbeat interval before next try
    pr->pause();
    break;
  case MsgUnreachable:
		// During optimistic replication, if the remote becomes unreachable,
		// there is huge probability that a MsgApp is lost.
    if (pr->state_ == ProgressStateReplicate) {
      pr->becomeProbe();
    }
    logger->Debugf(__FILE__, __LINE__, "%x failed to send message to %x because it is unreachable [%s]",
      r->id_, msg.from(), pr->String().c_str());
    break;
  case MsgTransferLeader:
    leadTransferee = msg.from();
    lastLeadTransferee = r->leadTransferee_;
    if (lastLeadTransferee != kNone) {
      if (lastLeadTransferee == leadTransferee) {
        logger->Infof(__FILE__, __LINE__,
          "%x [term %llu] transfer leadership to %x is in progress, ignores request to same node %x",
          r->id_, r->term_, leadTransferee, leadTransferee);
        return;
      }
      r->abortLeaderTransfer();
      logger->Infof(__FILE__, __LINE__,
        "%x [term %d] abort previous transferring leadership to %x",
        r->id_, r->term_, leadTransferee);
    }
    if (leadTransferee == r->id_) {
      logger->Debugf(__FILE__, __LINE__,
        "%x is already leader. Ignored transferring leadership to self",
        r->id_);
      return;
    }
    // Transfer leadership to third party.
    logger->Infof(__FILE__, __LINE__,
      "%x [term %llu] starts to transfer leadership to %x",
      r->id_, r->term_, leadTransferee);
    // Transfer leadership should be finished in one electionTimeout, so reset r.electionElapsed.
    r->electionElapsed_ = 0;
    r->leadTransferee_ = leadTransferee;
    if (pr->match_ == r->raftLog_->lastIndex()) {
      r->sendTimeoutNow(leadTransferee);
      logger->Infof(__FILE__, __LINE__,
        "%x sends MsgTimeoutNow to %x immediately as %x already has up-to-date log",
        r->id_, leadTransferee, leadTransferee);
    } else {
      r->sendAppend(leadTransferee);
    }
    break;
  }
}

// stepCandidate is shared by StateCandidate and StatePreCandidate; the difference is
// whether they respond to MsgVoteResp or MsgPreVoteResp.
void 
stepCandidate(raft* r, const Message& msg) {
  // Only handle vote responses corresponding to our candidacy (while in
  // StateCandidate, we may get stale MsgPreVoteResp messages in this term from
  // our pre-candidate state).
  MessageType voteRespType;
  if (r->state_ == StatePreCandidate) {
    voteRespType = MsgPreVoteResp;
  } else {
    voteRespType = MsgVoteResp;
  }
  int type = msg.type();
  int granted = 0;
  Logger* logger = r->logger_;

  if (type == voteRespType) {
    granted = r->poll(msg.from(), msg.type(), !msg.reject());
    logger->Infof(__FILE__, __LINE__, "%x [quorum:%llu] has received %d %s votes and %llu vote rejections",
      r->id_, r->quorum(), granted, kMsgString[type], r->votes_.size() - granted);
    if (granted == r->quorum()) {
      if (r->state_ == StatePreCandidate) {
        r->campaign(CampaignPreElection);
      } else {
        r->becomeLeader();
        r->bcastAppend();
      }
    } else if (r->quorum() == (int)r->votes_.size() - granted) {
      r->becomeFollower(r->term_, kNone);
    }
    return;
  }

  switch (type) {
  case MsgProp:
    logger->Infof(__FILE__, __LINE__, "%x no leader at term %llu; dropping proposal", r->id_, r->term_);
    return;
    break;
  case MsgApp:
    r->becomeFollower(r->term_, msg.from());
    r->handleAppendEntries(msg);
    break;
  case MsgHeartbeat:
    r->becomeFollower(r->term_, msg.from());
    r->handleHeartbeat(msg);
    break;
  case MsgTimeoutNow:
    logger->Debugf(__FILE__, __LINE__, "%x [term %llu state candidate] ignored MsgTimeoutNow from %x",
      r->id_, r->term_, msg.from());
    break;
  }
}

void 
stepFollower(raft* r, const Message& msg) {
  int type = msg.type();
  Message *n;
  Logger* logger = r->logger_;

  switch (type) {
  case MsgProp:
    if (r->leader_ == kNone) {
      return;
    }
    n = cloneMessage(msg);
    n->set_to(r->leader_);
    r->send(n);
    break;
  case MsgApp:
    r->electionElapsed_ = 0;
    r->leader_ = msg.from();
    r->handleAppendEntries(msg);
    break;
  case MsgHeartbeat:
    r->electionElapsed_ = 0;
    r->leader_ = msg.from();
    r->handleHeartbeat(msg);
    break;
  case MsgSnap:
    r->electionElapsed_ = 0;
    r->leader_ = msg.from();
    r->handleSnapshot(msg);
    break;
  case MsgTransferLeader:
    if (r->leader_ == kNone) {
      logger->Infof(__FILE__, __LINE__,
        "%x no leader at term %llu; dropping leader transfer msg",
        r->id_, r->term_);
      return;
    }
    n = cloneMessage(msg);
    n->set_to(r->leader_);
    r->send(n);
    break;
  case MsgTimeoutNow:
    if (r->promotable()) {
      logger->Infof(__FILE__, __LINE__,
        "%x [term %llu] received MsgTimeoutNow from %x and starts an election to get leadership.",
        r->id_, r->term_, msg.from());
 			// Leadership transfers never use pre-vote even if r.preVote is true; we
			// know we are not recovering from a partition so there is no need for the
			// extra round trip.
      r->campaign(CampaignTransfer);
    } else {
      logger->Infof(__FILE__, __LINE__,
        "%x received MsgTimeoutNow from %x but is not promotable",
        r->id_, msg.from());
    }
    break;
  case MsgReadIndex:
    if (r->leader_ == kNone) {
      logger->Infof(__FILE__, __LINE__, "%x no leader at term %llu; dropping index reading msg", r->id_, r->term_);
      return;
    }
    n = cloneMessage(msg);
    n->set_to(r->leader_);
    r->send(n);
    break;
  case MsgReadIndexResp:
    if (msg.entries_size() != 1) {
      logger->Errorf(__FILE__, __LINE__, "%x invalid format of MsgReadIndexResp from %x, entries count: %llu",
        r->id_, msg.from(), msg.entries_size());
      return;
    }
    r->readStates_.push_back(new ReadState(msg.index(), msg.entries(0).data()));
    break;
  }
}

// restore recovers the state machine from a snapshot. It restores the log and the
// configuration of state machine.
bool
raft::restore(const Snapshot& snapshot) {
  // ignore outdated snapshot
  if (snapshot.metadata().index() <= raftLog_->committed_) {
    return false;
  }

  // check if snapshot's [index,term] match log's [index,term]
  if (raftLog_->matchTerm(snapshot.metadata().index(), snapshot.metadata().term())) {
    logger_->Infof(__FILE__, __LINE__, "%x [commit: %llu, lastindex: %llu, lastterm: %llu] fast-forwarded commit to snapshot [index: %llu, term: %llu]",
      id_, raftLog_->committed_, raftLog_->lastIndex(), raftLog_->lastTerm(),
       snapshot.metadata().index(), snapshot.metadata().term());
    raftLog_->commitTo(snapshot.metadata().index());
    return false;
  }

  // below is the case use snapshot data to restore raft state machine

  logger_->Infof(__FILE__, __LINE__, "%x [commit: %llu, lastindex: %llu, lastterm: %llu] starts to restore snapshot [index: %llu, term: %llu]",
    id_, raftLog_->committed_, raftLog_->lastIndex(), raftLog_->lastTerm(),
    snapshot.metadata().index(), snapshot.metadata().term());

  raftLog_->restore(snapshot);
  progressMap_.clear();
  int i;
  // restore node progress
  for (i = 0; i < snapshot.metadata().conf_state().nodes_size(); ++i) {
    uint64_t node = snapshot.metadata().conf_state().nodes(i);
    uint64_t match = 0;
    uint64_t next = raftLog_->lastIndex() + 1; 
    if (node == id_) {
      match = next - 1;
    }
    setProgress(node, match, next);
    logger_->Infof(__FILE__, __LINE__, "%x restored progress of %x [%s]", id_, node, progressMap_[node]->String().c_str());
  }
  return true;
}

void
raft::handleSnapshot(const Message& msg) {
  uint64_t sindex = msg.snapshot().metadata().index();
  uint64_t sterm  = msg.snapshot().metadata().term();
  Message *resp = new Message;

  resp->set_to(msg.from());
  resp->set_type(MsgAppResp);
  if (restore(msg.snapshot())) {
    logger_->Infof(__FILE__, __LINE__, "%x [commit: %d] restored snapshot [index: %d, term: %d]",
      id_, raftLog_->committed_, sindex, sterm);
    // if success to restore from snapshot, return the last index
    resp->set_index(raftLog_->lastIndex());
  } else {
    logger_->Infof(__FILE__, __LINE__, "%x [commit: %d] ignored snapshot [index: %d, term: %d]",
      id_, raftLog_->committed_, sindex, sterm);
    // if failed to restore from snapshot, return the commit index
    resp->set_index(raftLog_->committed_);
  }
  send(resp);
}

void
raft::handleHeartbeat(const Message& msg) {
  // commit the msg commit index
  raftLog_->commitTo(msg.commit());
  Message *resp = new Message();
  resp->set_to(msg.from());
  resp->set_type(MsgHeartbeatResp);
  resp->set_context(msg.context());
  send(resp);
}

void
raft::handleAppendEntries(const Message& msg) {
  // is msg already outdated?
  if (msg.index() < raftLog_->committed_) {
    Message *resp = new Message();
    resp->set_to(msg.from());
    resp->set_type(MsgAppResp);
    resp->set_index(raftLog_->committed_);
    send(resp);
    return;
  }

  EntryVec entries;
  copyEntries(msg, &entries);
  uint64_t lasti;
  // check if msg append success?
  bool ret = raftLog_->maybeAppend(msg.index(), msg.logterm(), msg.commit(), entries, &lasti);
  if (ret) {
    Message *resp = new Message();
    resp->set_to(msg.from());
    resp->set_type(MsgAppResp);
    resp->set_index(lasti);
    send(resp);
  } else {
    uint64_t term;
    int err = raftLog_->term(msg.index(), &term);
    logger_->Debugf(__FILE__, __LINE__,
      "%x [logterm: %llu, index: %llu] rejected msgApp [logterm: %llu, index: %llu] from %x",
      id_, raftLog_->zeroTermOnErrCompacted(term, err), msg.index(), msg.logterm(), msg.index(), msg.from());
    Message *resp = new Message();
    resp->set_to(msg.from());
    resp->set_type(MsgAppResp);
    resp->set_index(msg.index());
    resp->set_reject(true);
    resp->set_rejecthint(raftLog_->lastIndex());
    send(resp);
  }
}

void
raft::setProgress(uint64_t id, uint64_t match, uint64_t next) {
  if (progressMap_[id] != NULL)  {
    delete progressMap_[id];
  }
  progressMap_[id] = new Progress(next, maxInfilght_, logger_);
  progressMap_[id]->match_ = match;
}

void
raft::delProgress(uint64_t id) {
  delete progressMap_[id];
  progressMap_.erase(id);
}

void
raft::abortLeaderTransfer() {
  leadTransferee_ = kNone;
}

void
raft::addNode(uint64_t id) {
  pendingConf_ = false;
  if (progressMap_.find(id) != progressMap_.end()) {
    return;
  }
  setProgress(id, 0, raftLog_->lastIndex() + 1);
}

void
raft::removeNode(uint64_t id) {
  delProgress(id);
  pendingConf_ = false;

  // do not try to commit or abort transferring if there is no nodes in the cluster.
  if (progressMap_.empty()) {
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

void
raft::readMessages(MessageVec *msgs) {
  *msgs = outMsgs_;
  outMsgs_.clear();
}

void
raft::sendTimeoutNow(uint64_t to) {
  Message *msg = new Message();
  msg->set_to(to);
  msg->set_type(MsgTimeoutNow);
  send(msg);
}

void
raft::resetPendingConf() {
  pendingConf_ = false;
}

//TODO:
static void 
validateConfig(const Config *config) {

}

raft* 
newRaft(const Config *config) {
  validateConfig(config);

  raftLog *rl = newLog(config->storage, config->logger);
  HardState hs;
  ConfState cs;
  Logger *logger = config->logger;
  vector<uint64_t> peers = config->peers;
  int err;
  size_t i;
  
  // init hs,cs from storage
  err = config->storage->InitialState(&hs, &cs);
  if (!SUCCESS(err)) {
    logger->Fatalf(__FILE__, __LINE__, "storage InitialState fail: %s", GetErrorString(err)); 
  }

  if (cs.nodes_size() > 0) {
    if (peers.size() > 0) {
      logger->Fatalf(__FILE__, __LINE__, "cannot specify both newRaft(peers) and ConfState.Nodes)");
    }
    peers.clear();
    for (i = 0; (int)i < cs.nodes_size(); ++i) {
      peers.push_back(cs.nodes(i));
    }
  }

  // init the progressMap_
  raft *r = new raft(config, rl);
  for (i = 0; i < peers.size(); ++i) {
    r->progressMap_[peers[i]] = new Progress(1, r->maxInfilght_, logger);
  }

  // load hardState if needed.
  if (!isHardStateEqual(hs, kEmptyHardState)) {
    r->loadState(hs);
  }
  if (config->applied > 0) {
    rl->appliedTo(config->applied);
  }

  // whenever start a node,change to follower state
  r->becomeFollower(r->term_, kNone);

  vector<string> peerStrs;
  map<uint64_t, Progress*>::const_iterator iter;
  char tmp[32];
  for (iter = r->progressMap_.begin(); iter != r->progressMap_.end(); ++iter) {
    snprintf(tmp, sizeof(tmp), "%llu", iter->first);
    peerStrs.push_back(tmp);
  }
  string nodeStr = joinStrings(peerStrs, ",");

  r->logger_->Infof(__FILE__, __LINE__,
    "newRaft %llu [peers: [%s], term: %llu, commit: %llu, applied: %llu, lastindex: %llu, lastterm: %llu]",
    r->id_, nodeStr.c_str(), r->term_, rl->committed_, rl->applied_, rl->lastIndex(), rl->lastTerm());
  return r;
}

}; // namespace libraft