include_directories( "${CMAKE_SOURCE_DIR}/libraries/blockchain/include" )

add_executable( bts_create_key bts_create_key.cpp )
target_link_libraries( bts_create_key fc bts_blockchain )
add_executable( bts_create_genesis bts_create_genesis.cpp )
target_link_libraries( bts_create_genesis fc bts_blockchain )
