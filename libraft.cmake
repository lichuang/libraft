set(libraft_files
  src/base/condition.cc    
  src/base/event_loop.cc 
  src/base/event.cc   
  src/base/mailbox.cc   
  src/base/signaler.cc 
  src/base/spin_lock.cc   
  src/base/thread.cc  
)

add_library(raft 
  ${raft_SHARED_OR_STATIC}
  ${libraft_files}
)
