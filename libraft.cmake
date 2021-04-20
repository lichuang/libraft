set(libraft_files
  src/proto/raft.pb.cc 

  src/base/default_logger.cc 
  src/base/crc32c.cc
  src/base/mutex.cc
  src/base/io_buffer.cc 
  src/base/util.cc 

  src/core/node.cc 
  src/core/progress.cc 
  src/core/raft.cc 
  src/core/read_only.cc 

  src/storage/log.cc    
  src/storage/memory_storage.cc      
  src/storage/unstable_log.cc  
)

add_library(raft 
  ${raft_SHARED_OR_STATIC}
  ${libraft_files}
)
