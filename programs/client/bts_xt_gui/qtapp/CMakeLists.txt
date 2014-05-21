cmake_minimum_required(VERSION 2.8.11)

message("-- Configuring bts_xt_gui")

project(btc_xt_gui)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)

# Find the QtWidgets library
find_package(Qt5Core REQUIRED)
find_package(Qt5Gui REQUIRED)
find_package(Qt5Widgets REQUIRED)
find_package(Qt5WebKit REQUIRED)
find_package(Qt5WebKitWidgets REQUIRED)

#message(${Qt5WebKitWidgets_INCLUDE_DIRS})

include_directories(${CMAKE_SOURCE_DIR} ${Qt5WebKitWidgets_INCLUDE_DIRS})

# Tell CMake to create the helloworld executable
add_executable(bts_xt_gui WIN32 main.cpp bts_xt_thread.cpp bts_xt_thread.h html5viewer/html5viewer.cpp)

# Use the Widgets module from Qt 5.
target_link_libraries(bts_xt_gui Qt5::Widgets Qt5::WebKit Qt5::WebKitWidgets
  bts_wallet bts_rpc bts_cli bts_blockchain bts_db bts_net bts_client fc ${READLINE_LIBRARIES} ${CMAKE_DL_LIBS} ${PLATFORM_SPECIFIC_LIBS} ${crypto_library} ${ZLIB_LIBRARY})
