if( WIN32 )
  set( PLATFORM_SPECIFIC_LIBS WS2_32 )
  add_definitions(-DBOOST_TEST_DYN_LINK) 
endif( WIN32 )

add_executable( blockchain_tests blockchain_tests.cpp )
target_link_libraries( blockchain_tests bts_blockchain fc ${BOOST_LIBRARIES}  ${PLATFORM_SPECIFIC_LIBS} ${rt_library} ${pthread_library} ${CMAKE_DL_LIBS} )
