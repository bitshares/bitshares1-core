##################################################################################################
# Magic to get the genesis.json converted to a .cpp file and compiled into the bts_client library
set(genesis_json "${CMAKE_CURRENT_SOURCE_DIR}/genesis_bts.json")

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

source_group("Genesis JSON Files" FILES ${genesis_json} ${genesis_prologue} ${genesis_epilogue})

##################################################################################################
# Regular stuff for the bts_client library

file(GLOB HEADERS "include/bts/blockchain/*.hpp")

add_library( bts_blockchain
             fork_blocks.cpp
             types.cpp
             asset.cpp
             address.cpp
             pts_address.cpp
             extended_address.cpp
             transaction_creation_state.cpp

             property_record.cpp
             account_record.cpp
             asset_record.cpp
             slate_record.cpp
             balance_record.cpp
             transaction_record.cpp
             burn_record.cpp
             status_record.cpp
             feed_record.cpp
             market_records.cpp
             slot_record.cpp

             transaction.cpp
             transaction_evaluation_state.cpp
             block.cpp

             operations.cpp
             account_operations_v1.cpp
             account_operations.cpp
             slate_operations.cpp
             withdraw_types.cpp
             balance_operations_v1.cpp
             balance_operations_v2.cpp
             balance_operations_v3.cpp
             balance_operations.cpp
             asset_operations.cpp
             feed_operations_v1.cpp
             feed_operations.cpp
             market_operations_v1.cpp
             market_operations_v2.cpp
             market_operations_v3.cpp
             market_operations_v4.cpp
             market_operations_v5.cpp
             market_operations.cpp
             data_operations.cpp

             time.cpp
             chain_interface_v1.cpp
             chain_interface_v2.cpp
             chain_interface.cpp
             chain_database_v1.cpp
             chain_database_v2.cpp
             chain_database.cpp
             pending_chain_state.cpp
             market_engine_v1.cpp
             market_engine_v2.cpp
             market_engine_v3.cpp
             market_engine_v4.cpp
             market_engine_v5.cpp
             market_engine_v6.cpp
             market_engine_v7.cpp
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
  if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # compiling this file with optimizations enabled causes internal compiler error in gcc 4.9 - 4.9.1
	# see https://github.com/BitShares/bitshares/issues/774, remove this when
	# the bug in gcc is fixed
    set_source_files_properties(chain_database.cpp PROPERTIES COMPILE_FLAGS "-O0")
  endif()
endif()

target_link_libraries( bts_blockchain fc bts_db leveldb )
target_include_directories( bts_blockchain
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

if (USE_PCH)
  set_target_properties(bts_blockchain PROPERTIES COTIRE_ADD_UNITY_BUILD FALSE)
  cotire(bts_blockchain)
endif(USE_PCH)
