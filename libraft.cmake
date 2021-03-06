set(libraft_files
  src/proto/raft.pb.cc 
  src/proto/record.pb.cc

  src/base/crc32c.cc
  src/base/file_system_adaptor.cc
  src/base/io_buffer.cc
  src/base/logger.cc
  src/base/mutex.cc
  src/base/util.cc 

  src/core/node.cc
  src/core/fsm_caller.cc
  src/core/progress.cc 
  src/core/raft.cc 
  src/core/read_only.cc 

  src/io/buffer_io_reader.cc

  src/storage/log.cc    
  src/storage/memory_storage.cc      
  src/storage/unstable_log.cc 

  src/wal/decoder.cc
  src/wal/encoder.cc
  src/wal/wal.cc
)

add_library(raft 
  ${raft_SHARED_OR_STATIC}
  ${libraft_files}
)
