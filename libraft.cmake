set(libraft_files
  src/util/array.c

  src/storage/unstable_log.c
)

add_library(raft 
  ${raft_SHARED_OR_STATIC}
  ${libraft_files}
)
