/*
 * Copyright (C) lichuang
 */

#pragma once

#include "base/entity.h"
#include "base/typedef.h"

namespace libraft {

class Endpoint;
class IDataHandler;

// base class for socket entity
class SessionEntity : public IEntity {
public:
  SessionEntity(IDataHandler *handler, const Endpoint&, fd_t);

protected:
  IDataHandler *handler_;
};
};