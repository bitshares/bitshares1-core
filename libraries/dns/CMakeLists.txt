include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/db/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/blockchain/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/wallet/include" )

add_library( bts_dns
             outputs.cpp
             dns_db.cpp
             dns_transaction_validator.cpp
             dns_wallet.cpp
           )

target_link_libraries( bts_dns bts_wallet fc bts_db bts_blockchain leveldb )
