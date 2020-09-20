
add_executable ( entity_test
  test/entity_test.cc
)

target_link_libraries (entity_test PRIVATE raft gtest event pthread protobuf gflags)

add_executable ( buffer_test
  test/buffer_test.cc
)

target_link_libraries (buffer_test PRIVATE raft gtest event pthread protobuf gflags)

add_executable ( server_test
  test/server_test.cc
)

target_link_libraries (server_test PRIVATE raft gtest event pthread protobuf gflags)

add_executable ( rpc_test
  test/rpc_test.cc
  test/echo.pb.cc
)

target_link_libraries (rpc_test PRIVATE raft gtest event pthread protobuf gflags)