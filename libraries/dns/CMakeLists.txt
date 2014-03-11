include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/include" )
include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/../db/include" )
add_library( bts_dns
             outputs.cpp
             dns_db.cpp
           )


target_link_libraries( bts_dns bts_blockchain fc bts_db leveldb )
