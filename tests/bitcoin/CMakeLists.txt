if( UNIX AND NOT APPLE )
  set(rt_library rt )
endif()

include_directories( ${CMAKE_SOURCE_DIR}/libraries/bitcoin/include )

add_executable( wallet_import_tests wallet_import_tests.cpp )
target_link_libraries( wallet_import_tests bitcoin bts_blockchain bts_wallet fc )
