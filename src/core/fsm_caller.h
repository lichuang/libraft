/*
 * Copyright (C) lichuang
 */

#ifndef __LIBRAFT_FSM_CALLER_H__
#define __LIBRAFT_FSM_CALLER_H__

namespace libraft {

class Node;
class StateMachine;
struct Ready;

class FsmCaller {
public:
  FsmCaller(StateMachine*, Node*);

  void on_call_fsm(Ready* ready);

private:
  StateMachine* fsm_;
  Node* node_;
};

};  // namespace libraft

#endif  // __LIBRAFT_FSM_CALLER_H__
