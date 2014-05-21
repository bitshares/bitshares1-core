include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/db/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/fc/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/client/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/wallet/include" )
include_directories( "${CMAKE_SOURCE_DIR}/vendor/miniupnp" )

file(GLOB HEADERS "include/bts/net/*.hpp")
    
set(SOURCES node.cpp
            stcp_socket.cpp
            core_messages.cpp
            peer_database.cpp
            upnp.cpp
            message_oriented_connection.cpp)

add_library( bts_net ${SOURCES} ${HEADERS} )

target_link_libraries( bts_net fc bts_db leveldb )
