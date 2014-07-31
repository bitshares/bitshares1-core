file(GLOB HEADERS "include/bts/mail/*.hpp")

if (THREADED_P2P)
  message(STATUS "Enabling dedicated thread for P2P code")
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DP2P_IN_DEDICATED_THREAD")
endif()
    
set(SOURCES message.cpp server.cpp )

add_library( bts_mail ${SOURCES} ${HEADERS} )

target_link_libraries( bts_mail 
  PUBLIC fc bts_wallet bts_db bts_utilities leveldb bts_api bts_rpc_stubs upnpc-static)
target_include_directories( bts_mail 
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
# currently depends on client for definitions, remove this
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../client/include"
# Looks like bts_mail and bts_rpc also have cycle deps
          "${CMAKE_CURRENT_SOURCE_DIR}/../rpc/include"
# later, move upnp include into target_include_directories for upnp
"${CMAKE_CURRENT_SOURCE_DIR}/../../vendor/miniupnp"
)

if (USE_PCH)
  set_target_properties(bts_mail PROPERTIES COTIRE_ADD_UNITY_BUILD FALSE)
  cotire(bts_mail)
endif(USE_PCH)
