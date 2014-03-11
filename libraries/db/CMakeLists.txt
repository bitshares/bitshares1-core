include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/include" )
add_library( bts_db upgrade_leveldb.cpp )
target_link_libraries( bts_db fc leveldb )
