include_directories( ${CMAKE_SOURCE_DIR}/libraries/wallet/include ) 
include_directories( ${CMAKE_SOURCE_DIR}/libraries/blockchain/include )
include_directories( ${CMAKE_SOURCE_DIR}/libraries/dns/include )
include_directories( ${CMAKE_SOURCE_DIR}/libraries/db/include )

add_executable( blockchain_tests blockchain_tests.cpp )
target_link_libraries( blockchain_tests bts_wallet bts_blockchain bitcoin_import fc ${BOOST_LIBRARIES} ${OPENSSL_LIBRARIES} ${crypto_library})

#add_executable( dns_tests dns_tests.cpp )
#target_link_libraries( dns_tests bts_wallet bts_dns bts_blockchain fc ${BOOST_LIBRARIES} ${OPENSSL_LIBRARIES} ${CRYPTO_LIB} bts_wallet)

include_directories( ${CMAKE_SOURCE_DIR}/libraries/net/include )
include_directories( ${CMAKE_SOURCE_DIR}/libraries/client/include )
add_executable( simple_net_test_client simple_net_test_client.cpp )
target_link_libraries( simple_net_test_client bts_client bts_net bts_blockchain fc ${BOOST_LIBRARIES} ${OPENSSL_LIBRARIES} ${crypto_library} )

