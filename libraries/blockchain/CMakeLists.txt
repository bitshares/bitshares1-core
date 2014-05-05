include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/include" )
include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/../db/include" )

file(GLOB HEADERS "include/bts/blockchain/*.hpp")

add_library( bts_blockchain
             small_hash.cpp
             difficulty.cpp
             momentum.cpp
             address.cpp
             pts_address.cpp
             asset.cpp
             outputs.cpp
             transaction.cpp
             block.cpp
             transaction_validator.cpp
             chain_database.cpp
             ${HEADERS}
           )

target_link_libraries( bts_blockchain fc bts_db leveldb )
