set(private_include_directories)

# This library depends upon BDB, if BDB is not found then it compiles out support for BDB 
FIND_PACKAGE( BerkeleyDB )
if( NOT ${BerkeleyDB_FOUND} )
  message( WARNING "Missing required BerkeleyDB package, Bitcoin Core Wallet Imports Disabled")
  set( sources bitcoin_stub.cpp )
else()
  message( STATUS "Enabling Bitcoin Core Wallet Imports")
  list(APPEND private_include_directories ${BDB_INCLUDE_DIR})
  #set( sources armory.cpp bitcoin.cpp electrum.cpp multibit.cpp )
  set( sources bitcoin.cpp electrum.cpp )
endif()

file(GLOB HEADERS "include/bts/*.hpp")

add_library( bitcoin ${sources} ${HEADERS})
target_link_libraries( bitcoin PUBLIC fc PRIVATE ${BDB_LIBRARIES} bts_blockchain)
target_include_directories( bitcoin 
  PRIVATE ${private_include_directories} "${CMAKE_CURRENT_SOURCE_DIR}/../wallet/include"
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
