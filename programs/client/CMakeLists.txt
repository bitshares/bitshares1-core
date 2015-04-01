add_executable( ${CLI_CLIENT_EXECUTABLE_NAME} main.cpp )
if( UNIX AND NOT APPLE )
  set(rt_library rt )
endif()

find_package( Gperftools QUIET )
if( GPERFTOOLS_FOUND )
    message( STATUS "Found gperftools; compiling client with TCMalloc")
    list( APPEND PLATFORM_SPECIFIC_LIBS tcmalloc )
endif()

target_link_libraries( ${CLI_CLIENT_EXECUTABLE_NAME}
                       PRIVATE bitcoin bts_keyhotee bts_wallet bts_rpc bts_cli bts_blockchain bts_db bts_net bts_client bts_utilities fc ${CMAKE_DL_LIBS} ${PLATFORM_SPECIFIC_LIBS} )
target_include_directories( ${CLI_CLIENT_EXECUTABLE_NAME}
                            PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include" )

IF(WIN32 AND "${TARGET_ARCHITECTURE}" STREQUAL "x86")
  SET_TARGET_PROPERTIES(${CLI_CLIENT_EXECUTABLE_NAME} PROPERTIES LINK_FLAGS "/LARGEADDRESSAWARE")
ENDIF()

if (ENABLE_INSTALLER)
  install(TARGETS ${CLI_CLIENT_EXECUTABLE_NAME} RUNTIME DESTINATION bin)
endif(ENABLE_INSTALLER)
