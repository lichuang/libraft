
target_link_libraries (server_test PRIVATE raft gtest event pthread protobuf gflags)

add_executable ( rpc_test
  test/rpc_test.cc
  test/echo.pb.cc
)

target_link_libraries (rpc_test PRIVATE raft gtest event pthread protobuf gflags)