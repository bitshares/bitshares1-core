include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/fc/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/kid/include" )

add_executable( kids main.cpp )
target_link_libraries( kids bts_kid fc ${PLATFORM_SPECIFIC_LIBS} )
