file(GLOB HEADERS "include/bts/cli/*.hpp")

add_library( bts_cli 
             cli.cpp
             ${HEADERS})

target_link_libraries( bts_cli bts_client fc bts_db bts_rpc bts_wallet bts_blockchain leveldb )
target_include_directories( bts_cli 
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )