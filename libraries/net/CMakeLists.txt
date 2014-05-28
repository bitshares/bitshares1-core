include_directories( "${CMAKE_SOURCE_DIR}/libraries/fc/include" )

# currently depends on client for definitions, remove this
include_directories( "${CMAKE_SOURCE_DIR}/libraries/client/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/rpc/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/wallet/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/wallet/include" )

# later, move upnp include into target_include_directories for upnp
include_directories( "${CMAKE_SOURCE_DIR}/vendor/miniupnp" )

file(GLOB HEADERS "include/bts/net/*.hpp")
    
set(SOURCES node.cpp
            stcp_socket.cpp
            core_messages.cpp
            peer_database.cpp
            upnp.cpp
            message_oriented_connection.cpp)

add_library( bts_net ${SOURCES} ${HEADERS} )

target_link_libraries( bts_net fc bts_db bts_utilities leveldb )
target_include_directories( bts_net 
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )