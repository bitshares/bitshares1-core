include_directories( "${CMAKE_SOURCE_DIR}/libraries/wallet/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/rpc/include" )



set(genesis_json "${CMAKE_SOURCE_DIR}/tests/genesis.json")
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
                                              "--output-file=${generated_genesis_file}"
                   DEPENDS bts_genesis_to_cpp ${genesis_json} )



file(GLOB HEADERS "include/bts/client/*.hpp")

add_library( bts_client 
             client.cpp
             messages.cpp
             ${generated_genesis_file}
             ${genesis_json}
             ${HEADERS} )

target_link_libraries( bts_client  
                       PRIVATE bts_db bts_blockchain bitcoin fc leveldb
                       PUBLIC bts_api bts_net bts_wallet bts_rpc bts_rpc_stubs)
target_include_directories( bts_client 
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )
