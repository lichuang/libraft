
add_executable ( libraft_test
  test/log_test.cc
  test/main.cc
)

target_link_libraries (libraft_test PRIVATE raft gtest pthread protobuf gflags)