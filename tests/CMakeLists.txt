if( UNIX AND NOT APPLE )
  set(rt_library rt )
endif()

add_definitions( -DBTS_GLOBAL_API_LOG=1 )

IF(NOT Boost_UNIT_TEST_FRAMEWORK_LIBRARY MATCHES "\\.(a|lib)$")
IF(WIN32)
add_definitions(/DBOOST_TEST_DYN_LINK)
ELSE(WIN32)
add_definitions(-DBOOST_TEST_DYN_LINK)
ENDIF(WIN32)
ENDIF()

file(GLOB regression_tests_common_logs_files "${CMAKE_CURRENT_SOURCE_DIR}/regression_tests/_common_logs/*.log")
source_group("Regression Tests\\Common Logs" FILES ${regression_tests_common_logs_files})

file(GLOB regression_tests_titan_test_files "${CMAKE_CURRENT_SOURCE_DIR}/regression_tests/titan_test/test.config"
                                            "${CMAKE_CURRENT_SOURCE_DIR}/regression_tests/titan_test/*.log")
source_group("Regression Tests\\Titan Test" FILES ${regression_tests_titan_test_files})

file(GLOB regression_tests_register_accounts_as_delegate_files "${CMAKE_CURRENT_SOURCE_DIR}/regression_tests/register_accounts_as_delegate_test/test.config"
                                                               "${CMAKE_CURRENT_SOURCE_DIR}/regression_tests/register_accounts_as_delegate_test/*.log")
source_group("Regression Tests\\Register Accounts as Delegates" FILES ${regression_tests_register_accounts_as_delegate_files})

file(GLOB regression_tests_invalid_names_test_files "${CMAKE_CURRENT_SOURCE_DIR}/regression_tests/invalid_names_test/test.config"
                                             "${CMAKE_CURRENT_SOURCE_DIR}/regression_tests/invalid_names_test/*.log")
source_group("Regression Tests\\Invalid Names Test" FILES ${regression_tests_invalid_names_test_files})

file(GLOB regression_tests_simple_test_files "${CMAKE_CURRENT_SOURCE_DIR}/regression_tests/simple_test/test.config"
                                             "${CMAKE_CURRENT_SOURCE_DIR}/regression_tests/simple_test/*.log")
source_group("Regression Tests\\Simple Test" FILES ${regression_tests_simple_test_files})

file(GLOB regression_tests_transfer_test_files "${CMAKE_CURRENT_SOURCE_DIR}/regression_tests/transfer_test/test.config"
                                               "${CMAKE_CURRENT_SOURCE_DIR}/regression_tests/transfer_test/*.log")
source_group("Regression Tests\\Transfer Test" FILES ${regression_tests_transfer_test_files})

file(GLOB regression_tests_two_client_test_files "${CMAKE_CURRENT_SOURCE_DIR}/regression_tests/two_client_test/test.config"
                                                 "${CMAKE_CURRENT_SOURCE_DIR}/regression_tests/two_client_test/*.log")
source_group("Regression Tests\\Two Client Test" FILES ${regression_tests_two_client_test_files})

file(GLOB regression_tests_dev_test_files "${CMAKE_CURRENT_SOURCE_DIR}/regression_tests/_tests_under_development/dev_test/test.config"
                                          "${CMAKE_CURRENT_SOURCE_DIR}/regression_tests/_tests_under_development/dev_test/*.log")
source_group("Regression Tests\\Dev Test" FILES ${regression_tests_dev_test_files})

file(GLOB regression_tests_wallet_asset_test_files "${CMAKE_CURRENT_SOURCE_DIR}/regression_tests/wallet_asset_test/test.config"
                                                   "${CMAKE_CURRENT_SOURCE_DIR}/regression_tests/wallet_asset_test/*.log")
source_group("Regression Tests\\Wallet Asset Test" FILES ${regression_tests_wallet_asset_test_files})

set(regression_test_files ${regression_tests_common_logs_files}
                          ${regression_tests_titan_test_files}
                          ${regression_tests_register_accounts_as_delegate_files}
                          ${regression_tests_invalid_name_test_files}
                          ${regression_tests_simple_test_files}
                          ${regression_tests_transfer_test_files}
                          ${regression_tests_two_client_test_files}
                          ${regression_tests_wallet_asset_test_files}
                          ${regression_tests_dev_test_files})

add_executable( wallet_tests wallet_tests.cpp ${regression_test_files})
target_link_libraries( wallet_tests bts_client bts_cli bts_wallet bts_blockchain bts_net bts_utilities deterministic_openssl_rand bitcoin fc )

add_executable( dev_tests dev_tests.cpp )
target_link_libraries( dev_tests bts_client bts_cli bts_wallet bts_blockchain bts_net bts_utilities deterministic_openssl_rand bitcoin fc )

add_executable( nathan_tests nathan_tests.cpp )
target_link_libraries( nathan_tests bts_client bts_cli bts_wallet bts_blockchain bts_net bts_utilities deterministic_openssl_rand bitcoin fc )

#add_executable( server_node server_node.cpp )
#target_link_libraries( server_node bts_client bts_network bts_net fc bts_cli )

#add_executable( client_node client_node.cpp )
#target_link_libraries( client_node bts_client bts_network bts_net fc bts_cli )

include_directories( ${CMAKE_SOURCE_DIR}/libraries/client/include )

if( WIN32 )
    set( DB_VERSION 60 )
    message( STATUS "Configuring Bitshares on WIN32")
    set( ZLIB_LIBRARIES "" )

    set(BOOST_ROOT $ENV{BOOST_ROOT})
    set(Boost_USE_DEBUG_PYTHON OFF)
    set(Boost_USE_MULTITHREADED ON)
    set(BOOST_ALL_DYN_LINK ON) # force dynamic linking for all libraries
    find_package( Boost 1.54 REQUIRED COMPONENTS
                  thread
                  date_time
                  system
                  filesystem
                  program_options
                  serialization
                  chrono
                  unit_test_framework
                  context
                  coroutine
                  locale )

   # message(STATUS "boost date_time ${Boost_DATE_TIME_LIBRARY}")
   # message(STATUS "boost libraries ${Boost_LIBRARIES}")
endif (WIN32)

#add_executable( deterministic_signature_test deterministic_signature_test.cpp)
#target_link_libraries( deterministic_signature_test bts_utilities deterministic_openssl_rand fc )


#if( false )
#   add_executable( simple_net_test_client simple_net_test_client.cpp )
#   if( WIN32 )
#       target_compile_definitions(simple_net_test_client PUBLIC BOOST_ALL_NO_LIB BOOST_ALL_DYN_LINK)
#   endif (WIN32)
#   target_link_libraries( simple_net_test_client bts_client bts_net bts_blockchain fc )
#endif (false )

include_directories( ${CMAKE_SOURCE_DIR}/libraries/rpc/include )

if( false )
add_executable( bitshares_client_tests bitshares_client_tests.cpp )
if( WIN32 )
#   target_compile_definitions(bitshares_client_tests PUBLIC BOOST_ALL_NO_LIB BOOST_ALL_DYN_LINK)
set( RPCRT4 Rpcrt4 )
endif (WIN32)
target_link_libraries( bitshares_client_tests bts_client bts_rpc bts_net bts_wallet bts_blockchain fc )
add_dependencies(bitshares_client_tests bitshares_client)
endif (false )

#add_executable( recover recover.cpp )
#target_link_libraries( recover bts_blockchain fc bts_utilities)

add_subdirectory( bitcoin )
add_subdirectory( drltc_tests )

