include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/include" )

file(GLOB HEADERS "include/bts/db/*.hpp")

add_library( bts_db upgrade_leveldb.cpp ${HEADERS} )
target_link_libraries( bts_db fc leveldb )
