file(GLOB HEADERS "include/bts/client/*.hpp")

add_library( bts_client 
             client.cpp
             messages.cpp
             ${HEADERS} )

target_link_libraries( bts_client  
                       PRIVATE bts_db bts_blockchain bitcoin fc leveldb
                       PUBLIC bts_api bts_net bts_wallet bts_cli bts_rpc bts_rpc_stubs)
target_include_directories( bts_client 
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )
