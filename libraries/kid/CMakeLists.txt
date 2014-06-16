file(GLOB HEADERS "include/bts/kid/*.hpp")

add_library( bts_kid kid_server.cpp ${HEADERS})
target_link_libraries(bts_kid
  PUBLIC fc
  PRIVATE bts_db bts_blockchain leveldb
)

target_include_directories(bts_kid
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

