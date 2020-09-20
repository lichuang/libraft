set(libraft_files
  src/base/buffer.cc    
  src/base/entity.cc    
  src/base/event_loop.cc 
  src/base/event.cc   
  src/base/mailbox.cc   
  src/base/signaler.cc 
  src/base/server.cc 
  src/base/time.cc  
  src/base/worker.cc  
  src/base/worker_pool.cc  

  src/base/log.cc  
  src/base/logger.cc  

  src/net/service.cc 
  src/net/net.cc 
  src/net/socket.cc 
  src/net/service.cc 
  src/net/session_entity.cc 
  src/net/service_entity.cc 

  src/net/rpc/rpc_service.cc
  src/net/rpc/packet_parser.cc

  src/util/hash.cc  
  src/util/string.cc  
)

add_library(raft 
  ${raft_SHARED_OR_STATIC}
  ${libraft_files}
)
