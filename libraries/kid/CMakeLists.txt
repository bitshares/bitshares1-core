include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/db/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/blockchain/include" )

add_library( bts_kid kid_server.cpp )
target_link_libraries( bts_kid fc bts_db bts_blockchain leveldb )
