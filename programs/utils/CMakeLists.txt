if( UNIX AND NOT APPLE )
  set(rt_library rt )
endif()

add_executable( bts_create_key bts_create_key.cpp )
target_link_libraries( bts_create_key fc bts_blockchain bts_utilities)

add_executable( bts_create_genesis bts_create_genesis.cpp )
target_link_libraries( bts_create_genesis fc bts_blockchain bts_utilities)

add_executable( key_to_wif key_to_wif.cpp )
target_link_libraries( key_to_wif fc bts_blockchain bts_utilities)

add_executable( bts_genesis_to_bin bts_genesis_to_bin.cpp )
target_link_libraries( bts_genesis_to_bin fc bts_blockchain)

add_executable( bts_genesis_to_cpp bts_genesis_to_cpp.cpp )
target_link_libraries( bts_genesis_to_cpp fc bts_utilities)

add_executable( map_bts_network map_bts_network.cpp )
target_link_libraries( map_bts_network fc bts_net bts_client)