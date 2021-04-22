
add_executable ( libraft_test  
  test/main.c

  test/array_test.c
)

target_link_libraries (libraft_test PRIVATE raft)