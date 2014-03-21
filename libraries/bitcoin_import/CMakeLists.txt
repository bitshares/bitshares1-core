include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/fc/include" )
# This library depends upon BDB, if BDB is not found then it compiles out support for BDB 
if( NOT ${BerkeleyDB_FOUND} )
  message( WARNING "Missing required BerkeleyDB package, Bitcoin Core Wallet Imports Disabled")
  set( sources import_bitcoin_wallet_stub.cpp )
else()
  message( STATUS "Enabling Bitcoin Core Wallet Imports Disabled")
  set( sources import_bitcoin_wallet.cpp )
endif()

add_library( bitcoin_import ${sources} )
target_link_libraries( bitcoin_import  fc ${BDB_LIBRARIES} )
