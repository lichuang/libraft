
add_executable ( libraft_test  
  test/crc32c_test.cc
  test/log_test.cc
  test/main.cc
  test/memory_storage_test.cc
  test/node_test.cc
  test/io_buffer_test.cc     
  test/progress_test.cc
  test/raft_flow_controller_test.cc
  test/raft_paper_test.cc
  test/raft_snap_test.cc
  test/raft_test_util.cc
  test/raft_test.cc 
  test/unstable_log_test.cc     

  #test/record_test.cc      
)

target_link_libraries (libraft_test PRIVATE raft gtest pthread protobuf gflags)