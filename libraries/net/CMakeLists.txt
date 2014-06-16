file(GLOB HEADERS "include/bts/net/*.hpp")
    
set(SOURCES node.cpp
            stcp_socket.cpp
            core_messages.cpp
            peer_database.cpp
            upnp.cpp
            message_oriented_connection.cpp)

add_library( bts_net ${SOURCES} ${HEADERS} )

target_link_libraries( bts_net 
  PUBLIC fc bts_wallet bts_db bts_utilities leveldb bts_api bts_rpc_stubs upnpc-static)
target_include_directories( bts_net 
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
# currently depends on client for definitions, remove this
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../client/include"
# Looks like bts_net and bts_rpc also have cycle deps
          "${CMAKE_CURRENT_SOURCE_DIR}/../rpc/include"
# later, move upnp include into target_include_directories for upnp
"${CMAKE_CURRENT_SOURCE_DIR}/../../vendor/miniupnp"
)

if (USE_PCH)
  set_target_properties(bts_net PROPERTIES COTIRE_ADD_UNITY_BUILD FALSE)
  cotire(bts_net)
endif(USE_PCH)
