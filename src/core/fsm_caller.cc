/*
 * Copyright (C) lichuang
 */

#include "libraft.h"
#include "base/util.h"
#include "core/fsm_caller.h"
#include "core/node.h"

namespace libraft {

FsmCaller::FsmCaller(StateMachine* fsm, Node* node)
  : fsm_(fsm),
    node_(node) {

}

void 
FsmCaller::on_call_fsm(Ready* ready) {
  if (fsm_ == NULL) { // fsm == NULL means in test mode
    (void)node_;
    //node_->Advance();
    return;
  }
  if (!isEmptySoftState(ready->softState)) {
    fsm_->on_soft_state_changed(ready->softState);
  }
}

};  // namespace libraft