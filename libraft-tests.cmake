
add_executable ( entity_test
  test/entity_test.cc
)

target_link_libraries (entity_test PRIVATE raft gtest event pthread protobuf gflags)

