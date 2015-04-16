file(GLOB HEADERS "include/bts/client/*.hpp")
file(GLOB APIS "*_api.cpp")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/include/bts/client/build_info.hpp.in" "${CMAKE_CURRENT_BINARY_DIR}/include/bts/client/build_info.hpp" @ONLY)

add_definitions( -DBTS_GLOBAL_API_LOG=1 )

add_library( bts_client
             client.cpp
             messages.cpp
             notifier.cpp
             api_logger.cpp
             ${APIS}
             ${HEADERS}
             "${CMAKE_CURRENT_BINARY_DIR}/include/bts/client/build_info.hpp" )

if(MSVC)
  set_source_files_properties(client.cpp PROPERTIES COMPILE_FLAGS "/bigobj")
endif(MSVC)

target_link_libraries( bts_client
                       PRIVATE bts_db bts_blockchain bitcoin fc leveldb
                       PUBLIC bts_api bts_net bts_wallet bts_cli bts_rpc bts_rpc_stubs )
target_include_directories( bts_client
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
                            PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/include" )
