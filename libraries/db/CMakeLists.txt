file(GLOB HEADERS "include/bts/db/*.hpp")
add_library( bts_db upgrade_leveldb.cpp ${HEADERS} )
target_link_libraries( bts_db fc leveldb )
target_include_directories( bts_db PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )
