
add_executable ( libraft_test  
  test/log_test.cc
  test/main.cc
  test/memory_storage_test.cc
  test/node_test.cc
  test/progress_test.cc
  test/raft_paper_test.cc
  test/raft_test_util.cc
  test/raft_test.cc    
)

target_link_libraries (libraft_test PRIVATE raft gtest pthread protobuf gflags)