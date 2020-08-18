set(libraft_files
  src/base/condition.cc  
  src/base/mailbox.cc   
  src/base/signaler.cc 
  src/base/spin_lock.cc   
  src/base/thread.cc  
)

add_library(serverkit 
  ${serverkit_SHARED_OR_STATIC}
  ${libraft_files}
)
