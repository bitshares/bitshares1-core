include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/db/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/blockchain/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/wallet/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/client/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/net/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/rpc/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/cli/include" )

add_executable( bts_xt_client main.cpp )
target_link_libraries( bts_xt_client bts_wallet bts_rpc bts_cli bts_blockchain bts_db bts_net bts_client fc ${READLINE_LIBRARIES} ${CMAKE_DL_LIBS} ${crypto_library} ${ZLIB_LIBRARY} )

add_executable( bts_xt_server server.cpp chain_server.cpp chain_connection.cpp )
target_link_libraries( bts_xt_server bts_wallet bts_rpc bts_cli bts_blockchain bts_db bts_net bts_client fc ${READLINE_LIBRARIES} ${CMAKE_DL_LIBS} ${crypto_library} ${ZLIB_LIBRARY} )
