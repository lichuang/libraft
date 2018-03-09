#ifndef __NODE_H__
#define __NODE_H__

#include "libraft.h"

class Raft;

class NodeImpl : public Node {
public:
  NodeImpl(Config *config);
  ~NodeImpl();

  virtual void Tick(Ready **ready);
private:

private:
  Raft *raft_;
  Logger *logger_;
};

#endif  // __NODE_H__
