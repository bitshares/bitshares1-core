include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/include" )
include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/../db/include" )
add_library( bts_blockchain 
             asset.cpp
             outputs.cpp
             transaction.cpp
             block.cpp
             transaction_validator.cpp
             chain_database.cpp
           )

target_link_libraries( bts_blockchain fc bts_db leveldb )
