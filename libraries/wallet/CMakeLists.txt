file(GLOB HEADERS "include/bts/wallet/*.hpp")

add_library( bts_wallet
             wallet_records.cpp
             wallet_db.cpp
             bitcoin.cpp
             transaction_builder.cpp
             transaction_ledger.cpp
             transaction_ledger_experimental.cpp
             login.cpp
             wallet.cpp
             ${HEADERS}
           )

target_link_libraries( bts_wallet fc bts_db bts_utilities bts_blockchain leveldb bitcoin bts_keyhotee bts_cli )
target_include_directories( bts_wallet
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_BINARY_DIR}/include" )

configure_file( "${CMAKE_CURRENT_SOURCE_DIR}/include/bts/wallet/url.hpp.in" "${CMAKE_CURRENT_BINARY_DIR}/include/bts/wallet/url.hpp" )


if(MSVC)
  set_source_files_properties(wallet.cpp PROPERTIES COMPILE_FLAGS "/bigobj")
endif()

if (USE_PCH)
  set_target_properties(bts_wallet PROPERTIES COTIRE_ADD_UNITY_BUILD FALSE)
  cotire(bts_wallet)
endif(USE_PCH)
