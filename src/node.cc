#include "node.h"
#include "raft.h"
#include <unistd.h>


NodeImpl::NodeImpl(Config *config)
  : raft_(newRaft(config))
  , logger_(config->logger) {
}

NodeImpl::~NodeImpl() {
}

void NodeImpl::Tick(Ready **ready) {
}

Node* StartNode(Config *config) {
  Node* node = new NodeImpl(config);
  
  return node;
}
