
add_executable( multest multest.cpp )
target_link_libraries( multest fc )
target_include_directories( multest
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/../.." )
