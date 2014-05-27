
include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/utilities/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/db/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/blockchain/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/wallet/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/net/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/client/include" )

file(GLOB HEADERS "include/bts/api/*.hpp")

add_executable( bts_api_generator 
                bts_api_generator.cpp
                ${HEADERS} )
target_include_directories( bts_api_generator
                            PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include" 
                                    "${CMAKE_SOURCE_DIR}/libraries/utilities/include"
                                    "${CMAKE_SOURCE_DIR}/libraries/db/include"
                                    "${CMAKE_SOURCE_DIR}/libraries/blockchain/include"
                                    "${CMAKE_SOURCE_DIR}/libraries/wallet/include"
                                    "${CMAKE_SOURCE_DIR}/libraries/net/include"
                                    "${CMAKE_SOURCE_DIR}/libraries/client/include" )

target_link_libraries( bts_api_generator fc bts_utilities leveldb )

#add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/.cpp")