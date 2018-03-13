#ifndef __NODE_H__
#define __NODE_H__

#include "libraft.h"

struct raft;

class NodeImpl : public Node {
public:
  NodeImpl(Config *config);
  ~NodeImpl();

  virtual void Tick(Ready **ready);
private:

private:
  raft *raft_;
  Logger *logger_;
};

#endif  // __NODE_H__
