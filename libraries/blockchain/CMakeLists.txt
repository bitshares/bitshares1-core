##################################################################################################
# Magic to get the genesis.json converted to a .cpp file and compiled into the bts_client library
set(genesis_json "${CMAKE_CURRENT_SOURCE_DIR}/genesis_btsx.json")
source_group("Genesis JSON Files" FILES ${genesis_json})

set(generated_genesis_file "${CMAKE_CURRENT_BINARY_DIR}/genesis_json.cpp")
set(rpc_stubs_output_dir "${CMAKE_BINARY_DIR}/libraries/rpc_stubs")
set(generated_rpc_stubs_files "${rpc_stubs_output_dir}/common_api_rpc_server.cpp" 
                              "${rpc_stubs_output_dir}/common_api_rpc_client.cpp" 
                              "${rpc_stubs_output_dir}/common_api_client.cpp" 
                              "${rpc_stubs_output_dir}/include/bts/rpc_stubs/common_api_rpc_server.hpp" 
                              "${rpc_stubs_output_dir}/include/bts/rpc_stubs/common_api_rpc_client.hpp"
                              "${rpc_stubs_output_dir}/include/bts/rpc_stubs/common_api_client.hpp"
                              "${rpc_stubs_output_dir}/include/bts/rpc_stubs/common_api_overrides.ipp")

add_custom_command(OUTPUT ${generated_genesis_file} 
                   COMMAND bts_genesis_to_cpp "--genesis-json=${genesis_json}" 
                                              "--output-file=${generated_genesis_file}.new"
                   COMMAND ${CMAKE_COMMAND} -E copy_if_different "${generated_genesis_file}.new" "${generated_genesis_file}"
                   COMMAND ${CMAKE_COMMAND} -E remove "${generated_genesis_file}.new"
                   DEPENDS bts_genesis_to_cpp ${genesis_json} )

                   
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
             ${generated_genesis_file}
             ${genesis_json}
             ${HEADERS}
           )

if(MSVC)
  set_source_files_properties(${generated_genesis_file} PROPERTIES COMPILE_FLAGS "/bigobj /Od")
else()
  set_source_files_properties(${generated_genesis_file} PROPERTIES COMPILE_FLAGS "-O0")
endif()

target_link_libraries( bts_blockchain fc bts_db leveldb )
target_include_directories( bts_blockchain 
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )
if (USE_PCH)
  set_target_properties(bts_blockchain PROPERTIES COTIRE_ADD_UNITY_BUILD FALSE)
  cotire(bts_blockchain)
endif(USE_PCH)
