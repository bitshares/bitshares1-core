file(GLOB HEADERS "include/bts/wallet/*.hpp")

add_library( bts_wallet
             wallet_records.cpp
             wallet_db.cpp
             wallet.cpp 
             ${HEADERS}
           )

target_link_libraries( bts_wallet fc bts_db bts_utilities bts_blockchain leveldb bitcoin bts_keyhotee)
target_include_directories( bts_wallet 
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

if (USE_PCH)
  set_target_properties(bts_wallet PROPERTIES COTIRE_ADD_UNITY_BUILD FALSE)
  cotire(bts_wallet)
endif(USE_PCH)
