include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/include" )

add_executable( p2p_client main.cpp )
target_link_libraries( p2p_client bts_dns_p2p bts_dns bitcoin bts_wallet bts_rpc bts_cli
  bts_blockchain bts_db bts_net bts_client fc ${READLINE_LIBRARIES} ${CMAKE_DL_LIBS}
  ${PLATFORM_SPECIFIC_LIBS} ${crypto_library} ${ZLIB_LIBRARY} )
target_include_directoris(p2p_client PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
add_executable( p2p_server server.cpp  )
target_link_libraries( p2p_server bts_dns_p2p bts_dns bts_wallet bts_rpc bts_cli bts_blockchain
  bts_db bts_net bts_client fc ${READLINE_LIBRARIES} ${CMAKE_DL_LIBS} ${PLATFORM_SPECIFIC_LIBS}
  ${crypto_library} ${ZLIB_LIBRARY} )
target_include_directoris(p2p_server PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
