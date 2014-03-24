include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/db/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/blockchain/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/bitcoin_import/include" )

add_library( bts_wallet 
             wallet.cpp
             extended_address.cpp
           )

target_link_libraries( bts_wallet fc bts_db bts_blockchain leveldb bitcoin_import)
