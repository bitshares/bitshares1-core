include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/include" )
include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/../db/include" )

file(GLOB HEADERS "include/bts/blockchain/*.hpp")

add_library( bts_blockchain
             time.cpp
             extended_address.cpp
             address.cpp
             pts_address.cpp
             asset.cpp
             operations.cpp
             withdraw_types.cpp
             pending_chain_state.cpp
             transaction.cpp
             chain_interface.cpp
             block.cpp
             chain_database.cpp
             ${HEADERS}
           )

target_link_libraries( bts_blockchain fc bts_db leveldb )
