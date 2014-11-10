
add_executable( serial_tests serial.cpp serial_tester.cpp serial_test_ecc.cpp )
target_link_libraries( serial_tests deterministic_openssl_rand bts_client bts_cli bts_wallet bts_blockchain bts_net bitcoin fc )
target_include_directories( serial_tests
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/../.." )
