# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)

# Find Qt
find_package(Qt5Core REQUIRED)

include_directories( "${CMAKE_SOURCE_DIR}/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/blockchain/include" )
include_directories( "${CMAKE_SOURCE_DIR}/libraries/utilities/include" )
include_directories( "${CMAKE_SOURCE_DIR}/programs/qt_wallet" )

ADD_DEFINITIONS(-DQT_NO_KEYWORDS)

set( SOURCES
  main.cpp
  update_utility.cpp
)
add_executable( web_update_utility ${SOURCES} )
target_link_libraries( web_update_utility fc bts_blockchain bts_utilities Qt5::Core )
