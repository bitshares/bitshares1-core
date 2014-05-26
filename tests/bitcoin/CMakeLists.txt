include_directories( ${CMAKE_SOURCE_DIR}/libraries/bitcoin/include )

add_executable( armory_wallet_tests armory_wallet_tests.cpp )
target_link_libraries( armory_wallet_tests bitcoin bts_blockchain bts_wallet fc ${BOOST_LIBRARIES} ${OPENSSL_LIBRARIES} ${PLATFORM_SPECIFIC_LIBS} ${crypto_library})

add_executable( bitcoin_wallet_tests bitcoin_wallet_tests.cpp )
target_link_libraries( bitcoin_wallet_tests bitcoin bts_blockchain bts_wallet fc ${BOOST_LIBRARIES} ${OPENSSL_LIBRARIES} ${PLATFORM_SPECIFIC_LIBS} ${crypto_library})

add_executable( electrum_wallet_tests electrum_wallet_tests.cpp )
target_link_libraries( electrum_wallet_tests bitcoin bts_blockchain bts_wallet fc ${BOOST_LIBRARIES} ${OPENSSL_LIBRARIES} ${PLATFORM_SPECIFIC_LIBS} ${crypto_library})

add_executable( multibit_wallet_tests multibit_wallet_tests.cpp )
target_link_libraries( multibit_wallet_tests bitcoin bts_blockchain bts_wallet fc ${BOOST_LIBRARIES} ${OPENSSL_LIBRARIES} ${PLATFORM_SPECIFIC_LIBS} ${crypto_library})
