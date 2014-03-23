include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/db/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/fc/include" )

add_library( bts_net 
             chain_server.cpp
             chain_client.cpp
             chain_connection.cpp
             node.cpp
             stcp_socket.cpp
           )

target_link_libraries( bts_net fc bts_db leveldb )
