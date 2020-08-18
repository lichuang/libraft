/*
 * Copyright (C) codedump
 */

#ifndef __LIBRAFT_BASE_DEFINE_H__
#define __LIBRAFT_BASE_DEFINE_H__

#define BEGIN_NAMESPACE namespace libraft {
#define END_NAMESPACE   };

// A macro to disallow the copy constructor and operator= functions
// This should be used in the private: declarations for a class
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&);               \
  void operator=(const TypeName&)
  
#endif // __LIBRAFT_BASE_DEFINE_H__