include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/blockchain/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/cli/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/client/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/db/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/dns/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/net/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/rpc/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/wallet/include" )

file(GLOB HEADERS "include/bts/dns/p2p/*.hpp")

add_library( bts_dns_p2p
             p2p_transaction_validator.cpp
             p2p_rpc_server.cpp
             p2p_cli.cpp
             ${HEADERS} )

target_link_libraries( bts_dns_p2p bts_dns bts_blockchain bts_db bts_rpc bts_wallet fc leveldb )
