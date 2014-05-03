include_directories( ${CMAKE_SOURCE_DIR}/libraries/dns/include )
include_directories( ${CMAKE_SOURCE_DIR}/libraries/dns/p2p/include )

add_executable( p2p_tests p2p_tests.cpp )
target_link_libraries( p2p_tests bts_dns_p2p bts_dns bts_wallet bts_blockchain bitcoin_import fc ${BOOST_LIBRARIES} ${OPENSSL_LIBRARIES} ${PLATFORM_SPECIFIC_LIBS} ${crypto_library})
