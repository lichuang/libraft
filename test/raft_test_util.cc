#include <stdlib.h>
#include <time.h>
#include "raft_test_util.h"
#include "raft.h"
#include "default_logger.h"
#include "util.h"

// nextEnts returns the appliable entries and updates the applied index
void nextEnts(raft *r, Storage *s, EntryVec *entries) {
  // Transfer all unstable entries to "stable" storage.
  EntryVec tmp;
  r->raftLog_->unstableEntries(&tmp); 
  s->Append(&tmp);
  r->raftLog_->stableTo(r->raftLog_->lastIndex(), r->raftLog_->lastTerm());

  r->raftLog_->nextEntries(entries);
  r->raftLog_->appliedTo(r->raftLog_->committed_);
}

string raftLogString(raftLog *log) {
  char buf[1024] = {'\0'};
  string str = "";

  snprintf(buf, sizeof(buf), "committed: %llu\n", log->committed_);
  str += buf;

  snprintf(buf, sizeof(buf), "applied: %llu\n", log->applied_);
  str += buf;

  EntryVec entries;
  log->allEntries(&entries);

  snprintf(buf, sizeof(buf), "entries size: %lu\n", entries.size());
  str += buf;

  int i;
  for (i = 0; i < entries.size(); ++i) {
    str += entryString(entries[i]);
  }

  return str;
}

bool operator < (const connem& c1, const connem& c2) {
  if (c1.from < c2.from) {
    return true;
  }
  if (c1.from == c2.from) {
    return c1.to < c2.to;
  }
  return false;
}

raftStateMachine::raftStateMachine(Config *c) {
  raft = newRaft(c);
}

raftStateMachine::raftStateMachine(struct raft *r)
  : raft(r) {
}

raftStateMachine::~raftStateMachine() {
  delete raft;
}

int raftStateMachine::step(const Message& msg) {
  return raft->step(msg);
}

void raftStateMachine::readMessages(vector<Message*> *msgs) {
  raft->readMessages(msgs);
}

void idsBySize(int size, vector<uint64_t>* ids) {
  int i = 0;
  for (i = 0; i < size; ++i) {
    ids->push_back(1 + i);
  }
}

// newNetworkWithConfig is like newNetwork but calls the given func to
// modify the configuration of any state machines it creates.
network* newNetworkWithConfig(ConfigFun fun, const vector<stateMachine*>& peers) {
  srand(time(NULL));
  int size = peers.size();
  vector<uint64_t> peerAddrs;
  idsBySize(size, &peerAddrs); 
  network *net = new network();
  MemoryStorage *s;
  Config *c;
  raftStateMachine *r;
  raft *rf;

  int i, j;
  for (i = 0; i < size; ++i) {
    stateMachine *p = peers[i];
    uint64_t id = peerAddrs[i];

    if (!p) {
      s = new MemoryStorage(&kDefaultLogger);
      net->storage[id] = s;
      c = newTestConfig(id, peerAddrs, 10, 1, s);
      if (fun) {
        fun(c);
      }
      r = new raftStateMachine(c);
      net->peers[id] = r;
      continue;
    }

    switch (p->type()) {
    case raftType:
      rf = (raft *)p->data();
      rf->id_ = id;
      for (j = 0; j < size; ++j) {
        rf->prs_[peerAddrs[j]] = new Progress(0, 256, &kDefaultLogger);
      }
      rf->reset(rf->term_);
      net->peers[id] = p;
      break;
    case blackHoleType:
      net->peers[id] = p;
      break;
    }
  }

  return net;
}

network* newNetwork(const vector<stateMachine*>& peers) {
  return newNetworkWithConfig(NULL, peers);
}

void network::send(vector<Message> *msgs) {
  while (!msgs->empty()) {
    const Message& msg = (*msgs)[0];
    stateMachine *sm = peers[msg.to()]; 
    sm->step(msg);
    vector<Message> out;
    vector<Message*> readMsgs;
    msgs->erase(msgs->begin(), msgs->begin() + 1);
    sm->readMessages(&readMsgs);
    filter(readMsgs, &out);
    msgs->insert(msgs->end(), out.begin(), out.end());
  }
}

void network::drop(uint64_t from, uint64_t to, int perc) {
  dropm[connem(from, to)] = perc;
}

void network::cut(uint64_t one, uint64_t other) {
  drop(one, other, 10);
  drop(other, one, 10);
}

void network::isolate(uint64_t id) {
  int i;
  for (i = 0; i < peers.size(); ++i) {
    uint64_t nid = i + 1;
    if (nid != id) {
      drop(id, nid, 10);
      drop(nid, id, 10);
    }
  }
}

void network::ignore(MessageType t) {
  ignorem[t] = true;
}

void network::recover() {
  dropm.clear();
  ignorem.clear();
}

void network::filter(const vector<Message *>& msgs, vector<Message> *out) {
  int i;
  for (i = 0; i < msgs.size(); ++i) {
    Message *msg = msgs[i];
    if (ignorem[msg->type()]) {
      continue;
    }

    int perc;
    switch (msg->type()) {
    case MsgHup:
      break;
    default:
      perc = dropm[connem(msg->from(), msg->to())];
      if (rand() % 10 < perc) {
        continue;
      }
    }

    out->push_back(*msg);
  }
}

raft* newTestRaft(uint64_t id, const vector<uint64_t>& peers, int election, int hb, Storage *s) {
  return newRaft(newTestConfig(id, peers, election, hb, s));
}

Config* newTestConfig(uint64_t id, const vector<uint64_t>& peers, int election, int hb, Storage *s) {
  Config *c = new Config();
  c->id = id;
  c->peers = peers;
  c->electionTick = election;
  c->heartbeatTick = hb;
  c->storage = s;
  c->maxSizePerMsg = noLimit;
  c->maxInflightMsgs = 256;
  c->logger = &kDefaultLogger;
  c->readOnlyOption = ReadOnlySafe;
  c->checkQuorum = false;
  return c;
}

