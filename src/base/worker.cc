/*
 * Copyright (C) lichuang
 */

#include <map>
#include <sys/prctl.h>
#include "base/bind_entity_msg.h"
#include "base/entity.h"
#include "base/entity_type.h"
#include "base/event.h"
#include "base/event_loop.h"
#include "base/log.h"
#include "base/message.h"
#include "base/mailbox.h"
#include "base/typedef.h"
#include "base/wait.h"
#include "base/worker.h"

using namespace std;

namespace libraft {

struct addTimerMsg: public IMessage {
public:
  addTimerMsg(ITimerEvent* event)
    : IMessage(kAddTimerMessage),
      event_(event) {
  }

  ITimerEvent* event_;
};

// the default entity class bind to a worker
class workerEntity : public IEntity {
public:
  workerEntity(Worker* w) : IEntity(kWorkerEntity) {
    w->AddEntity(this);
  }

  virtual ~workerEntity() {
  }

  void Handle(IMessage* m);

  void handleTimerMessage(IMessage *m) {
    ref_.worker_->addTimer(((addTimerMsg*)m)->event_);
  }

  void handleBindEntityMessage(IMessage *m) {
    IEntity *en = ((bindEntityMsg*)m)->entity_;
    en->afterBindToWorker(ref_.worker_);
  }  
};

void
workerEntity::Handle(IMessage *m) {
  MessageType typ = m->Type();

  switch (typ) {
  case kAddTimerMessage:
    handleTimerMessage(m);
    break;
  case kBindEntityMessage:
    handleBindEntityMessage(m);
    break;
  default:
    break;
  }
}

// TLS worker pointer
thread_local static Worker* gWorker = nullptr;

Worker::Worker(const string &name, threadType typ, bool isMain)
  : state_(kThreadNone),
    name_(name),
    thread_(nullptr),
    mailbox_(nullptr),
    ev_loop_(new EventLoop()),
    event_(nullptr),
    current_(0),
    worker_entity_(nullptr),
    current_msg_id_(0),
    current_timer_id_(0),
    type_(typ) {  
  mailbox_ = new Mailbox(this);
  // add mailbox signal fd into event loop
  fd_t fd = signaler_.Fd();
  event_ = new IOEvent(ev_loop_, fd, this);
  event_->EnableRead();

  // worker entity is the id 1 entity in each worker
  worker_entity_ = new workerEntity(this);

  // start worker thread
  if (isMain) {
    // save TLS worker pointer
    gWorker = this;
    init();
    return;
  }
  WaitGroup wg;
  wg.Add(1);
  thread_ = new std::thread(Worker::main, this, &wg);
  wg.Wait();
}

Worker::~Worker() {
  delete mailbox_;
  delete ev_loop_;
  delete event_;
  delete worker_entity_;
  delete thread_;
}

void 
Worker::AddEntity(IEntity* entity) {
  // AddEntity can only be called in current thread
  ASSERT(CurrentThread() == this);

  // register entity in worker
  EntityId id = ++current_;
  while (entities_.find(id) != entities_.end()) {
    id++;
  }
  current_ = id;

  entity->Bind(this, id);
  entities_[id] = entity;
}

void
Worker::onRead(IOEvent*) {
  signaler_.Recv();
  mailbox_->Recv();

  if (!Running()) {
    ev_loop_->Stop();
  }
}

void
Worker::onWrite(IOEvent*) {
  // nothing to do
}

MessageId 
Worker::NewMsgId() {
  return ++current_msg_id_;
}

void
Worker::process(IMessage *msg) {
  const EntityRef& dstRef = msg->DstRef();
  EntityId id = dstRef.id();

  EntityMap::iterator iter = entities_.find(id);
  if (iter == entities_.end()) {
    delete msg;
    return;
  }

  if (!msg->isResponse_) {
    iter->second->Handle(msg);
  } else {
    iter->second->HandleResponse(msg);
  }

  delete msg;
}

void 
Worker::notify() {
  signaler_.Send();
}

void
Worker::Send(IMessage *msg) {
  mailbox_->Send(msg);
}

void 
Worker::SendtoWorker(IMessage *msg) {
  msg->setDstEntiity(worker_entity_->Ref());
  mailbox_->Send(msg);
}

bool
Worker::runningInWorker() {
  return false;
  return Running() && CurrentThreadId() == Id();
}

void
Worker::addTimer(ITimerEvent* event) {  
  event->Start();
}

TimerEventId
Worker::newTimerEventId() {
  TimerEventId id = ++current_timer_id_;
  while (timer_event_map_.find(id) != timer_event_map_.end()) {
    ++id;
  }
  current_timer_id_ = id;
  return id;
}

TimerEventId 
Worker::newTimer(ITimerHandler* handler, const Duration& delay, bool once) {
  TimerEventId id = newTimerEventId();
  ITimerEvent *event = new ITimerEvent(ev_loop_, handler, delay, once, id);
  if (runningInWorker()) {
    addTimer(event);
  } else {
    IMessage *msg = new addTimerMsg(event);
    SendtoWorker(msg);
  }

  return id;
}

TimerEventId 
Worker::RunEvery(ITimerHandler* handler, const Duration& internal) {
  return newTimer(handler, internal, true);
}

TimerEventId 
Worker::RunOnce(ITimerHandler* handler, const Duration& delay) {
  return newTimer(handler, delay, false);
}

void
Worker::init() {
  state_ = kThreadRunning;
  ::prctl(PR_SET_NAME, name_.c_str());
}

void
Worker::Run() {
  init();
  ev_loop_->Run();
}

void 
Worker::Stop() {
  state_ = kThreadStopping;
  // notify event loop to stop
  signaler_.Send();
  thread_->join();
  mailbox_->Recv();
  state_ = kThreadStopped;
}

void 
Worker::main(Worker* worker, WaitGroup* wg) {
  // save TLS worker pointer
  gWorker = worker;    

  // notify thread has been created
  wg->Done();
  worker->Run();
}

void 
Sendto(const EntityRef& dstRef, IMessage* msg) {
  if (!gWorker) {
    initMainWorker();
  }  
  gWorker->worker_entity_->Sendto(dstRef, msg);
}

MessageId 
NewMsgId() {
  if (!gWorker) {
    initMainWorker();
  }  
  return gWorker->NewMsgId();
}

const string& 
CurrentThreadName() {
  if (!gWorker) {
    initMainWorker();
  }  
  return gWorker->String();
}

ThreadId CurrentThreadId() {
  if (!gWorker) {
    initMainWorker();
  }
  return gWorker->Id();
}

void 
initMainWorker() {
  new Worker("main", kMainThread, true);
}

Worker*
CurrentThread() {
  return gWorker;
}

bool 
InMainThread() {
  return gWorker->Type() == kMainThread;
}
};