if( UNIX AND NOT APPLE )
  set(rt_library rt )
endif()

add_executable( client_rpc_tests client_rpc_tests.cpp )

target_link_libraries( client_rpc_tests bitcoin bts_net bts_cli bts_blockchain bts_wallet fc )



