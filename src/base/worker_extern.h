/*
 * Copyright (C) lichuang
 */

#pragma once

// worker extern global function declarations
namespace libraft {
class EventLoop;
class Worker;
class IEntity;

extern void CreateWorkerPool(int);

extern void initMainWorker();
extern bool InMainThread();

extern Worker* CurrentThread();
extern EventLoop* CurrentEventLoop();

extern void BindEntity(IEntity*);
extern void DestroyEntity(IEntity*);
}