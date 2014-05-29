add_executable( bitshares_client main.cpp )
target_link_libraries( bitshares_client bitcoin bts_wallet bts_rpc bts_cli bts_blockchain bts_db bts_net bts_client bts_utilities fc ${CMAKE_DL_LIBS} ${PLATFORM_SPECIFIC_LIBS} ${OPENSSL_LIBRARIES} ${ZLIB_LIBRARY} upnpc-static )
target_include_directories( bitshares_client 
                            PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include" )
