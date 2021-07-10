/*
 * Copyright (C) lichuang
 */

#ifndef __LIBRAFT_IO_ERROR_H__
#define __LIBRAFT_IO_ERROR_H__

namespace libraft {

enum FileErrorCode {
  kOK = 0,
  kEOF = -1,
  kErrUnexpectedEOF = -2,
};

}; // namespace libraft

#endif // __LIBRAFT_IO_ERROR_H__