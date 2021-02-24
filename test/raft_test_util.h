/*
 * Copyright (C) lichuang
 */

#ifndef __LIBRAFT_RAFT_TEST_UTIL_H__
#define __LIBRAFT_RAFT_TEST_UTIL_H__

#include "libraft.h"
#include "core/raft.h"
#include "core/progress.h"
#include "storage/memory_storage.h"

using namespace libraft;

enum stateMachineType {
  raftType = 0,
  blackHoleType = 1
};

struct stateMachine {
  virtual ~stateMachine() {}

  virtual int step(const Message& ) = 0;
  virtual void readMessages(MessageVec *) = 0;

  virtual int type() = 0;
  virtual void* data() = 0;
};

struct connem {
  uint64_t from, to;

  bool operator == (const connem& c) {
    return from == c.from && to == c.to;
  }

  void operator = (const connem& c) {
    from = c.from;
    to = c.to;
  }

  connem(uint64_t from, uint64_t to)
    : from(from), to(to) {}
};

struct network {
  map<uint64_t, stateMachine*> peers;
  map<uint64_t, MemoryStorage*> storage;
  map<connem, int> dropm;
  map<MessageType, bool> ignorem;

  ~network();
  void send(vector<Message>* msgs);
  void drop(uint64_t from, uint64_t to, int perc);
  void cut(uint64_t one, uint64_t other);
  void isolate(uint64_t id);
  void ignore(MessageType t);
  void recover();
  void filter(const vector<Message *>& msg, vector<Message> *out);
};

struct raftStateMachine : public stateMachine {
  raftStateMachine(Config *c);
  raftStateMachine(raft *);
  virtual ~raftStateMachine();

  virtual int step(const Message& );
  virtual void readMessages(MessageVec *);

  virtual int type() { return raftType; }
  virtual void* data() { return raft; }

  Config* config;
  raft *raft;
};

struct blackHole : public stateMachine {
  blackHole() {}
  virtual ~blackHole() {}

  int step(const Message& ) { return OK; }
  void readMessages(MessageVec *) {}

  int type() { return blackHoleType; }
  void* data() { return NULL; } 
};

typedef void (*ConfigFun)(Config*);

extern Config* newTestConfig(uint64_t id, const vector<uint64_t>& peers, int election, int hb, Storage *s);
extern raft* newTestRaft(uint64_t id, const vector<uint64_t>& peers, int election, int hb, Storage *s);
extern network* newNetworkWithConfig(ConfigFun fun, const vector<stateMachine*>& peers);
extern network* newNetwork(const vector<stateMachine*>& peers);
extern void nextEnts(raft *r, MemoryStorage *s, EntryVec *entries);
extern string raftLogString(raftLog *log);
extern void idsBySize(int size, vector<uint64_t>* ids);

static inline Entry 
initEntry(uint64_t index=0, uint64_t term=0,const string data = "") { 
  Entry entry;
  entry.set_index(index);
  entry.set_term(term);
  entry.set_data(data);
  return entry;
}

static inline Message 
initMessage(uint64_t from=0, uint64_t to=0, const MessageType typ=MsgHup, EntryVec *entries = NULL, uint64_t index = 0) { 
    Message msg;
    msg.set_from(from);
    msg.set_to(to);
    msg.set_index(index);
    msg.set_type(typ);
    if (entries != NULL) {
      uint32_t i;
      for (i = 0; i < entries->size(); ++i) {
        *(msg.add_entries()) = (*entries)[i];
      }
    }
    return msg;
}

static inline Snapshot*
newSnapshot(uint64_t index = 0, uint64_t term = 0) { 
  Snapshot* s = new Snapshot();
  SnapshotMetadata *meta = s->mutable_metadata();
 
  if (index != 0) {
    meta->set_index(index);
  }
  if (term != 0) {
    meta->set_term(term);
  }  
  return s;
}

static inline Progress
initProgress(uint64_t next, int maxInfilght, Logger *logger, ProgressState state, uint64_t match, uint64_t pendingSnapshot = 0) { 
  Progress progress(next, maxInfilght, logger);
  progress.state_ = state;
  progress.match_ = match;
  progress.pendingSnapshot_ = pendingSnapshot;

  return progress;
}

#define SIZEOF_ARRAY(array) sizeof(array) / sizeof(array[0])

#endif  // __LIBRAFT_RAFT_TEST_UTIL_H__
