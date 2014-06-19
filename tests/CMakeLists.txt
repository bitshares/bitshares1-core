if( UNIX AND NOT APPLE )
  set(rt_library rt )
endif()

add_library( deterministic_openssl_rand deterministic_openssl_rand.cpp deterministic_openssl_rand.cpp)
target_link_libraries( deterministic_openssl_rand fc )

add_executable( wallet_tests wallet_tests.cpp )
target_link_libraries( wallet_tests deterministic_openssl_rand bts_client bts_cli bts_wallet bts_blockchain bts_net bitcoin fc )

add_executable( dev_tests dev_tests.cpp )
target_link_libraries( dev_tests deterministic_openssl_rand bts_client bts_cli bts_wallet bts_blockchain bts_net bitcoin fc )


include_directories( ${CMAKE_SOURCE_DIR}/libraries/client/include )

if( WIN32 )
    set( DB_VERSION 60 )
    message( STATUS "Configuring Bitshares on WIN32")
    set( ZLIB_LIBRARIES "" )
    set( LEVEL_DB_DIR vendor/leveldb-win )

    set(BOOST_ROOT $ENV{BOOST_ROOT})
    set(Boost_USE_DEBUG_PYTHON OFF)
    set(Boost_USE_MULTITHREADED ON)
    set(Boost_USE_STATIC_LIBS OFF)
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

   message("boost date_time ${Boost_DATE_TIME_LIBRARY}")
   message("boost libraries ${Boost_LIBRARIES}")
endif (WIN32)

add_executable( deterministic_signature_test deterministic_signature_test.cpp)
target_link_libraries( deterministic_signature_test deterministic_openssl_rand fc )


#if( false )
#   add_executable( simple_net_test_client simple_net_test_client.cpp )
#   if( WIN32 )
#       target_compile_definitions(simple_net_test_client PUBLIC BOOST_ALL_NO_LIB BOOST_ALL_DYN_LINK)
#   endif (WIN32)
#   target_link_libraries( simple_net_test_client bts_client bts_net bts_blockchain fc )
#endif (false )

include_directories( ${CMAKE_SOURCE_DIR}/libraries/rpc/include )

 
add_executable( bitshares_client_tests bitshares_client_tests.cpp )
if( WIN32 )
#   target_compile_definitions(bitshares_client_tests PUBLIC BOOST_ALL_NO_LIB BOOST_ALL_DYN_LINK)
   set( RPCRT4 Rpcrt4 )
endif (WIN32)
target_link_libraries( bitshares_client_tests bts_client bts_rpc bts_net bts_wallet bts_blockchain fc )
add_dependencies(bitshares_client_tests bitshares_client)

add_subdirectory( bitcoin )
add_subdirectory( client_rpc_tests )

if( false )
   add_subdirectory( dns )
endif (false )
