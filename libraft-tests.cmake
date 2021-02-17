
add_executable ( libraft_test
  test/raft_test_util.cc
  test/log_test.cc
  test/memory_storage_test.cc
  test/node_test.cc
  test/main.cc
)

target_link_libraries (libraft_test PRIVATE raft gtest pthread protobuf gflags)