file(GLOB HEADERS "include/bts/light_wallet/*.hpp")

add_library( bts_light_wallet
             light_wallet.cpp
             ${HEADERS}
           )

target_link_libraries( bts_light_wallet fc bts_utilities bts_blockchain bts_rpc_stubs )
target_include_directories( bts_light_wallet
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_BINARY_DIR}/include" )

if (USE_PCH)
  set_target_properties(bts_light_wallet PROPERTIES COTIRE_ADD_UNITY_BUILD FALSE)
  cotire(bts_light_wallet)
endif(USE_PCH)
