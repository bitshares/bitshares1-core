file(GLOB HEADERS "include/bts/rpc/*.hpp")

add_library( bts_rpc 
             rpc_server.cpp
             rpc_client.cpp
             ${HEADERS}
           )

target_link_libraries( bts_rpc 
                       PUBLIC bts_blockchain bts_rpc_stubs bts_api bts_wallet bts_client bts_net fc 
                       PRIVATE bts_db bts_utilities leveldb)
target_include_directories( bts_rpc 
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )
if (USE_PCH)
  set_target_properties(bts_rpc PROPERTIES COTIRE_ADD_UNITY_BUILD FALSE)
  cotire(bts_rpc)
endif(USE_PCH)