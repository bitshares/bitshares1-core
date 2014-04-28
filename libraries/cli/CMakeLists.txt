include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/db/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/blockchain/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/wallet/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/net/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/rpc/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/client/include" )

file(GLOB HEADERS "include/bts/cli/*.hpp")

add_library( bts_cli 
             cli.cpp
             ${HEADERS})

target_link_libraries( bts_cli bts_client fc bts_db bts_blockchain leveldb )
