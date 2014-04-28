include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/fc/include" )
# This library depends upon BDB, if BDB is not found then it compiles out support for BDB 
FIND_PACKAGE( BerkeleyDB )
if( NOT ${BerkeleyDB_FOUND} )
  message( WARNING "Missing required BerkeleyDB package, Bitcoin Core Wallet Imports Disabled")
  set( sources import_bitcoin_wallet_stub.cpp )
else()
  message( STATUS "Enabling Bitcoin Core Wallet Imports")
  include_directories( ${BDB_INCLUDE_DIR} )
  set( sources import_bitcoin_wallet.cpp )
endif()

file(GLOB HEADERS "include/bts/*.hpp")

add_library( bitcoin_import ${sources} ${HEADERS})
target_link_libraries( bitcoin_import  fc ${BDB_LIBRARIES} )
