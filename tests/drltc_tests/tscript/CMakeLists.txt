
add_executable( tscript tscript.cpp )
target_link_libraries( tscript bts_client bts_cli bts_wallet bts_blockchain bts_net bts_utilities deterministic_openssl_rand bitcoin fc )
target_include_directories( tscript
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/../.." )
