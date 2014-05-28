include_directories( "${CMAKE_SOURCE_DIR}/libraries/fc/include" )

add_executable( bts_api_generator 
                bts_api_generator.cpp )
target_include_directories( bts_api_generator
                            PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include" 
                                    "${CMAKE_SOURCE_DIR}/libraries/utilities/include"
                                    "${CMAKE_SOURCE_DIR}/libraries/db/include"
                                    "${CMAKE_SOURCE_DIR}/libraries/blockchain/include"
                                    "${CMAKE_SOURCE_DIR}/libraries/wallet/include"
                                    "${CMAKE_SOURCE_DIR}/libraries/net/include"
                                    "${CMAKE_SOURCE_DIR}/libraries/client/include" )
target_link_libraries( bts_api_generator fc bts_utilities leveldb )


set(json_description_files "${CMAKE_CURRENT_SOURCE_DIR}/network_api.json")

set(generated_api_files "${CMAKE_CURRENT_BINARY_DIR}/include/bts/api/common_api.hpp")
set(rpc_stub_output_dir "${CMAKE_BINARY_DIR}/libraries/rpc_stub")
set(generated_rpc_stub_files "${rpc_stub_output_dir}/common_api_server.cpp" 
                             "${rpc_stub_output_dir}/common_api_client.cpp" 
                             "${rpc_stub_output_dir}/include/bts/rpc_stub/common_api_server.hpp" 
                             "${rpc_stub_output_dir}/include/bts/rpc_stub/common_api_client.hpp")

add_custom_command(OUTPUT ${generated_api_files} ${generated_rpc_stub_files}
                   COMMAND bts_api_generator "--api-classname=common_api" 
                                             "--api-interface-output-dir=${CMAKE_CURRENT_BINARY_DIR}"
                                             "--rpc-stub-output-dir=${rpc_stub_output_dir}" ${json_description_files}
                   DEPENDS bts_api_generator ${json_description_files} )

# add an empty c++ file to the target to make cmake happy.  when we hit cmake 3.0, we can change this to
# an interface library and get rid of dummy.cpp
add_library(bts_api STATIC "${CMAKE_CURRENT_BINARY_DIR}/include/bts/api/common_api.hpp" "dummy.cpp")
target_include_directories(bts_api
                           INTERFACE "${CMAKE_CURRENT_BINARY_DIR}/include")
#set_target_properties(bts_api PROPERTIES LINKER_LANGUAGE CXX)


add_library(bts_rpc_stubs ${generated_rpc_stub_files})
target_link_libraries( bts_rpc_stubs bts_client fc)
target_include_directories(bts_rpc_stubs
                           PUBLIC "${rpc_stub_output_dir}/include")