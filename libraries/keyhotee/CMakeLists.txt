file(GLOB HEADERS "include/bts/keyhotee/*.hpp")

add_library( bts_keyhotee
             import_keyhotee_id.cpp
             ${HEADERS}
           )

target_link_libraries( bts_keyhotee fc bts_db bts_blockchain leveldb bitcoin)
target_include_directories( bts_keyhotee 
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )
