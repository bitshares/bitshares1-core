##################################################################################################
# Magic to get the genesis.json converted to a .cpp file and compiled into the bts_client library
set(genesis_json "${CMAKE_CURRENT_SOURCE_DIR}/genesis.json")

set(EMBED_GENESIS_STATE_AS_TEXT FALSE CACHE BOOL "If true, genesis.json is included in text form, causing slower startup times")

if (EMBED_GENESIS_STATE_AS_TEXT)
  set(genesis_prologue "${CMAKE_CURRENT_SOURCE_DIR}/genesis.prologue")
  set(genesis_epilogue "${CMAKE_CURRENT_SOURCE_DIR}/genesis.epilogue")

  set(generated_genesis_file "${CMAKE_CURRENT_BINARY_DIR}/genesis_json.cpp")

  add_custom_command(OUTPUT ${generated_genesis_file}
                     COMMAND bts_json_to_cpp    "--json=${genesis_json}"
                                                "--prologue=${genesis_prologue}"
                                                "--epilogue=${genesis_epilogue}"
                                                "--out=${generated_genesis_file}.new"
                     COMMAND ${CMAKE_COMMAND} -E copy_if_different "${generated_genesis_file}.new" "${generated_genesis_file}"
                     COMMAND ${CMAKE_COMMAND} -E remove "${generated_genesis_file}.new"
                     DEPENDS bts_json_to_cpp ${genesis_json} ${genesis_prologue} ${genesis_epilogue} )
else(EMBED_GENESIS_STATE_AS_TEXT)
  set(genesis_prologue "${CMAKE_CURRENT_SOURCE_DIR}/binary_genesis.prologue")
  set(genesis_epilogue "${CMAKE_CURRENT_SOURCE_DIR}/binary_genesis.epilogue")

  set(generated_genesis_file "${CMAKE_CURRENT_BINARY_DIR}/genesis_json.cpp")

  add_custom_command(OUTPUT ${generated_genesis_file}
                     COMMAND bts_genesis_to_bin "--json=${genesis_json}"
                                                "--prologue=${genesis_prologue}"
                                                "--epilogue=${genesis_epilogue}"
                                                "--source-out=${generated_genesis_file}.new"
                     COMMAND ${CMAKE_COMMAND} -E copy_if_different "${generated_genesis_file}.new" "${generated_genesis_file}"
                     COMMAND ${CMAKE_COMMAND} -E remove "${generated_genesis_file}.new"
                     DEPENDS bts_genesis_to_bin ${genesis_json} ${genesis_prologue} ${genesis_epilogue} )
endif(EMBED_GENESIS_STATE_AS_TEXT)

source_group("Genesis JSON Files" FILES ${genesis_json} ${genesis_prologue} ${genesis_epilogue})

##################################################################################################
# Regular stuff for the bts_client library

file(GLOB HEADERS "include/bts/blockchain/*.hpp")

add_library( bts_blockchain
             address.cpp
             pts_address.cpp
             extended_address.cpp
             types.cpp
             withdraw_types.cpp
             operations.cpp
             balance_operations.cpp
             account_operations.cpp
             delegate_slate.cpp
             asset_operations.cpp
             market_operations.cpp
#proposal_operations.cpp
             feed_operations.cpp
#fire_operation.cpp
             asset.cpp
             transaction.cpp
             time.cpp
             block.cpp
             transaction_evaluation_state.cpp
             account_record.cpp
             asset_record.cpp
             market_records.cpp
             chain_interface.cpp
             pending_chain_state.cpp
             chain_database.cpp
             market_engine.cpp
             ${generated_genesis_file}
             ${genesis_json}
             ${HEADERS}
           )

if(MSVC)
  set_source_files_properties(${generated_genesis_file} PROPERTIES COMPILE_FLAGS "/bigobj /Od")
  set_source_files_properties(chain_database.cpp PROPERTIES COMPILE_FLAGS "/bigobj")
else()
  set_source_files_properties(${generated_genesis_file} PROPERTIES COMPILE_FLAGS "-O0")
endif()

target_link_libraries( bts_blockchain fc bts_db leveldb )
target_include_directories( bts_blockchain
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

if (EMBED_GENESIS_STATE_AS_TEXT)
  target_compile_definitions( bts_blockchain PRIVATE EMBED_GENESIS_STATE_AS_TEXT )
endif (EMBED_GENESIS_STATE_AS_TEXT)

if (USE_PCH)
  set_target_properties(bts_blockchain PROPERTIES COTIRE_ADD_UNITY_BUILD FALSE)
  cotire(bts_blockchain)
endif(USE_PCH)
