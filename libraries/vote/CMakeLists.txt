file(GLOB HEADERS "include/bts/vm/*.hpp")
set(SOURCES ballot.cpp )

add_library( bts_vote ${SOURCES} ${HEADERS} )

target_link_libraries( bts_vote 
  PUBLIC fc bts_wallet bts_db bts_utilities leveldb bts_api bts_rpc_stubs upnpc-static)
target_include_directories( bts_vote 
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
# currently depends on client for definitions, remove this
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../client/include"
# Looks like bts_vote and bts_rpc also have cycle deps
          "${CMAKE_CURRENT_SOURCE_DIR}/../rpc/include"
)

if (USE_PCH)
  set_target_properties(bts_vote PROPERTIES COTIRE_ADD_UNITY_BUILD FALSE)
  cotire(bts_vote)
endif(USE_PCH)
