if( UNIX AND NOT APPLE )
  set(rt_library rt )
endif()

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../../libraries/bitcoin/include )

add_executable( btc_wallet_dump bitcoin_recovery.cpp )
target_link_libraries( btc_wallet_dump fc bts_blockchain bts_utilities bitcoin )

add_executable( bts_create_key bts_create_key.cpp )
target_link_libraries( bts_create_key fc bts_blockchain bts_utilities deterministic_openssl_rand )

add_executable( bts_create_genesis bts_create_genesis.cpp )
target_link_libraries( bts_create_genesis fc bts_blockchain bts_utilities)

add_executable( key_to_wif key_to_wif.cpp )
target_link_libraries( key_to_wif fc bts_blockchain bts_utilities)

add_executable( bts_key_info bts_key_info.cpp )
target_link_libraries( bts_key_info fc bts_blockchain bts_utilities)

add_executable( bts_convert_addresses bts_convert_addresses.cpp )
target_link_libraries( bts_convert_addresses fc bts_blockchain bts_utilities)

# I've added two small files here that are also compiled in bts_blockchain
# to avoid a circular dependency.  The circular dependency could be broken more cleanly
# by splitting bts_blockchain, but it doesn't seem worth it just for this
add_executable( bts_genesis_to_bin bts_genesis_to_bin.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../../libraries/blockchain/types.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../libraries/blockchain/pts_address.cpp )
target_link_libraries( bts_genesis_to_bin fc )
target_include_directories( bts_genesis_to_bin PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../libraries/blockchain/include" )

add_executable( bts_json_to_cpp bts_json_to_cpp.cpp )
target_link_libraries( bts_json_to_cpp fc bts_utilities)

add_executable( map_bts_network map_bts_network.cpp )
target_link_libraries( map_bts_network fc bts_net bts_client)

add_executable( pack_web pack_web.cpp )
target_link_libraries( pack_web fc )

add_executable( compute_item_hashes compute_item_hashes.cpp )
target_link_libraries( compute_item_hashes fc bts_net bts_client)

if( ${INCLUDE_QT_WALLET} )
  add_subdirectory( web_update_utility )
endif()
