file(GLOB HEADERS "include/bts/vote/*.hpp")

add_library( bts_vote 
             types.cpp
             identity_verifier.cpp
             ${HEADERS})

target_link_libraries( bts_vote 
                       PUBLIC bts_client fc bts_db bts_rpc bts_wallet bts_blockchain leveldb )
target_include_directories( bts_vote 
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )
