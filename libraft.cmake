set(libraft_files
  src/util/array.c
  src/util/log.c

  src/storage/memory_storage.c
  src/storage/unstable_log.c
)

add_library(raft 
  ${raft_SHARED_OR_STATIC}
  ${libraft_files}
)
