include_directories( "${CMAKE_SOURCE_DIR}/libraries/wallet/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/net/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/client/include" )

file(GLOB HEADERS "include/bts/rpc/*.hpp")

add_library( bts_rpc 
             rpc_server.cpp
             rpc_client.cpp
             ${HEADERS}
           )

target_link_libraries( bts_rpc 
                       PUBLIC bts_rpc_stubs bts_api 
                       PRIVATE bts_client fc bts_db bts_blockchain bts_utilities leveldb)
target_include_directories( bts_rpc 
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )