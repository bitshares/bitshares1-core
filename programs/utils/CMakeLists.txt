if( UNIX AND NOT APPLE )
  set(rt_library rt )
endif()

include_directories( ${CMAKE_SOURCE_DIRECTORY}/libraries/bitcoin/include )

add_executable( btc_wallet_dump bitcoin_recovery.cpp )
target_link_libraries( btc_wallet_dump fc bts_blockchain bts_utilities bitcoin )

add_executable( bts_create_key bts_create_key.cpp )
target_link_libraries( bts_create_key fc bts_blockchain bts_utilities)

add_executable( bts_create_genesis bts_create_genesis.cpp )
target_link_libraries( bts_create_genesis fc bts_blockchain bts_utilities)

add_executable( key_to_wif key_to_wif.cpp )
target_link_libraries( key_to_wif fc bts_blockchain bts_utilities)

add_executable( bts_genesis_to_bin bts_genesis_to_bin.cpp )
target_link_libraries( bts_genesis_to_bin fc bts_blockchain)

add_executable( bts_json_to_cpp bts_json_to_cpp.cpp )
target_link_libraries( bts_json_to_cpp fc bts_utilities)

add_executable( map_bts_network map_bts_network.cpp )
target_link_libraries( map_bts_network fc bts_net bts_client)

add_executable( pack_web pack_web.cpp )
target_link_libraries( pack_web fc )

add_executable( convert_btc_pubkey convert_btc_pubkey.cpp )
target_link_libraries( convert_btc_pubkey fc bts_blockchain bts_utilities)
