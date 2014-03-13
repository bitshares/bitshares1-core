include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/db/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/blockchain/include" )

add_library( bts_x 
             outputs.cpp
             btsx_db.cpp
             btsx_transaction_validator.cpp
           )
