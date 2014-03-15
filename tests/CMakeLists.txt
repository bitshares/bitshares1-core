include_directories( ${CMAKE_SOURCE_DIR}/libraries/wallet/include ) 
include_directories( ${CMAKE_SOURCE_DIR}/libraries/blockchain/include )
include_directories( ${CMAKE_SOURCE_DIR}/libraries/dns/include )
include_directories( ${CMAKE_SOURCE_DIR}/libraries/db/include )

if( WIN32 )
  set( PLATFORM_SPECIFIC_LIBS WS2_32 )
  add_definitions(-DBOOST_TEST_DYN_LINK) 
endif( WIN32 )

add_executable( blockchain_tests blockchain_tests.cpp )
target_link_libraries( blockchain_tests bts_wallet bts_blockchain fc ${BOOST_LIBRARIES} ${OPENSSL_LIBRARIES} crypto )


add_executable( dns_tests dns_tests.cpp )
target_link_libraries( dns_tests bts_wallet bts_dns bts_blockchain fc ${BOOST_LIBRARIES} ${OPENSSL_LIBRARIES} crypto bts_wallet)

 
