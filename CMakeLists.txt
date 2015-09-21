# Defines BitShares library target.
project( BitShares )
cmake_minimum_required( VERSION 2.8.12 )

# this variable is set to different values in each git branch:
# develop branch                => BitSharesTestNet
# devshares branch              => DevShares
# bitshares and master branches => BitShares
set( BLOCKCHAIN_NAME "BitShares" )

if( BLOCKCHAIN_NAME STREQUAL BitShares )
  set( CLI_CLIENT_EXECUTABLE_NAME bitshares_client )
  set( GUI_CLIENT_EXECUTABLE_NAME BitShares )
  set( CUSTOM_URL_SCHEME "bts" )
  set( INSTALLER_APP_ID "68ad7005-8eee-49c9-95ce-9eed97e5b347" )
elseif( BLOCKCHAIN_NAME STREQUAL DevShares )
  set( CLI_CLIENT_EXECUTABLE_NAME devshares_client )
  set( GUI_CLIENT_EXECUTABLE_NAME DevShares )
  set( CUSTOM_URL_SCHEME "dvs" )
  set( INSTALLER_APP_ID "6bc47c81-1c8a-461d-ae7b-d14f974b1846" )
elseif(BLOCKCHAIN_NAME STREQUAL BitSharesTestNet )
  set( CLI_CLIENT_EXECUTABLE_NAME bitsharestestnet_client )
  set( GUI_CLIENT_EXECUTABLE_NAME BitSharesTestNet )
  set( CUSTOM_URL_SCHEME "xts" )
  set( INSTALLER_APP_ID "3f87ba55-e899-4bcf-ad92-0a8074c931e6" )
else()
  message( FATAL_ERROR "Expected BLOCKCHAIN_NAME to be set to BitShares, DevShares, or BitSharesTestNet" )
endif()

# http://stackoverflow.com/a/18369825
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.8)
        message(FATAL_ERROR "GCC version must be at least 4.8!")
    endif()
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 3.3)
        message(FATAL_ERROR "Clang version must be at least 3.3!")
    endif()
endif()

list( APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules" )

set(CMAKE_EXPORT_COMPILE_COMMANDS "ON")

#set (ENABLE_INSTALLER 1)
#set (USE_PCH 1)

if (USE_PCH)
  include (cotire)
endif(USE_PCH)

list( APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/libraries/fc/CMakeModules" )
list( APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/libraries/fc/GitVersionGen" )
include( GetGitRevisionDescription )
get_git_head_revision( GIT_REFSPEC GIT_SHA2 )

SET(BOOST_COMPONENTS)
LIST(APPEND BOOST_COMPONENTS thread
                             date_time
                             system
                             filesystem
                             program_options
                             signals
                             serialization
                             chrono
                             unit_test_framework
                             context
                             locale)
SET( Boost_USE_STATIC_LIBS ON CACHE STRING "ON or OFF" )

IF( WIN32 )
  SET(BOOST_ROOT $ENV{BOOST_ROOT})
  set(Boost_USE_MULTITHREADED ON)
  set(BOOST_ALL_DYN_LINK OFF) # force dynamic linking for all libraries
ENDIF(WIN32)

FIND_PACKAGE(Boost 1.53 REQUIRED COMPONENTS ${BOOST_COMPONENTS})
# For Boost 1.53 on windows, coroutine was not in BOOST_LIBRARYDIR and do not need it to build,  but if boost versin >= 1.54, find coroutine otherwise will cause link errors
IF(NOT "${Boost_VERSION}" MATCHES "1.53(.*)")
   SET(BOOST_LIBRARIES_TEMP ${Boost_LIBRARIES})
   FIND_PACKAGE(Boost 1.54 REQUIRED COMPONENTS coroutine)
   LIST(APPEND BOOST_COMPONENTS coroutine)
   SET(Boost_LIBRARIES ${BOOST_LIBRARIES_TEMP} ${Boost_LIBRARIES})
ENDIF()

set( LEVEL_DB_DIR "${CMAKE_SOURCE_DIR}/libraries/leveldb" )

file( GLOB LEVEL_DB_SOURCES "${LEVEL_DB_DIR}/db/*.cc"
			    "${LEVEL_DB_DIR}/helpers/memenv/memenv.cc"
			    "${LEVEL_DB_DIR}/table/*.cc"
			    "${LEVEL_DB_DIR}/util/*.cc" )
foreach( filename ${LEVEL_DB_SOURCES} )
    if( ${filename} MATCHES ".*_test.cc" OR ${filename} MATCHES ".*_bench.cc" OR ${filename} MATCHES ".*_main.cc" )
	list( REMOVE_ITEM LEVEL_DB_SOURCES ${filename} )
    endif()
endforeach()
set(LEVELDB_BUILD_DEFINES)
set(LEVELDB_BUILD_LIBRARIES)
set(LEVELDB_BUILD_PRIVATE_INCLUDES "${LEVEL_DB_DIR}")

if( WIN32 )
    message( STATUS "Configuring BitShares on WIN32")
    set( DB_VERSION 60 )
    set( BDB_STATIC_LIBS 1 )

    set( ZLIB_LIBRARIES "" )
    SET( DEFAULT_EXECUTABLE_INSTALL_DIR bin/ )

    set(CRYPTO_LIB)

    #looks like this flag can have different default on some machines.
    SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /SAFESEH:NO")
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SAFESEH:NO")

    # Probably cmake has a bug and vcxproj generated for executable in Debug conf. has disabled debug info
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /DEBUG")

    # On windows tcl should be installed to the directory pointed by setenv.bat script
    SET(TCL_INCLUDE_PATH $ENV{TCL_ROOT}/include)
    MESSAGE(STATUS "tcl INCLUDE PATH: ${TCL_INCLUDE_PATH}")

    FIND_PACKAGE(TCL)
    MESSAGE(STATUS "tcl_library: ${TCL_LIBRARY}")

    SET(TCL_LIBS "optimized;${TCL_LIBRARY};debug;")
    get_filename_component(TCL_LIB_PATH "${TCL_LIBRARY}" PATH)
    get_filename_component(TCL_LIB_NAME "${TCL_LIBRARY}" NAME_WE)
    get_filename_component(TCL_LIB_EXT "${TCL_LIBRARY}" EXT)

    SET(TCL_LIBS "${TCL_LIBS}${TCL_LIB_PATH}/${TCL_LIB_NAME}g${TCL_LIB_EXT}")
    SET(TCL_LIBRARY ${TCL_LIBS})

    SET(LEVELDB_PORT_FILE "${LEVEL_DB_DIR}/port/port_win.cc" )
    list(APPEND LEVELDB_BUILD_DEFINES OS_WINDOWS LEVELDB_PLATFORM_WINDOWS )
    list(APPEND LEVELDB_BUILD_LIBRARIES shlwapi.lib)
    list(INSERT LEVELDB_BUILD_PRIVATE_INCLUDES 0 "${CMAKE_SOURCE_DIR}/libraries/leveldb-msvc/include")
else( WIN32 ) # Apple AND Linux
    SET(LEVELDB_PORT_FILE "${LEVEL_DB_DIR}/port/port_posix.cc" )

    list(APPEND LEVELDB_BUILD_DEFINES LEVELDB_PLATFORM_POSIX LEVELDB_ATOMIC_PRESENT)
    if( APPLE )
        list(APPEND LEVELDB_BUILD_DEFINES OS_MACOSX)
    else() # Linux
        list(APPEND LEVELDB_BUILD_DEFINES OS_LINUX)
        list(APPEND LEVELDB_BUILD_LIBRARIES pthread)
    endif()

    find_library(READLINE_LIBRARIES NAMES readline)
    find_path(READLINE_INCLUDE_DIR readline/readline.h)
    #if(NOT READLINE_INCLUDE_DIR OR NOT READLINE_LIBRARIES)
    #  MESSAGE(FATAL_ERROR "Could not find lib readline.")
    #endif()

    if( APPLE )
        # Apple Specific Options Here
        message( STATUS "Configuring BitShares on OS X" )
        set( CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++11 -stdlib=libc++ -Wall" )
    else( APPLE )
        # Linux Specific Options Here
        message( STATUS "Configuring BitShares on Linux" )
        set( CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++11 -Wall" )
        set( rt_library rt )
        set( pthread_library pthread)
        if ( NOT DEFINED crypto_library )
          # I'm not sure why this is here, I guess someone has openssl and can't detect it with find_package()?
          # if you have a normal install, you can define crypto_library to the empty string to avoid a build error
          set( crypto_library crypto)
        endif ()
        if ( FULL_STATIC_BUILD )
          set( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -static-libgcc")
        endif ( FULL_STATIC_BUILD )
    endif( APPLE )

    if( "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" )
        set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-builtin-memcmp" )
    endif()

    if( "${CMAKE_GENERATOR}" STREQUAL "Ninja" )
        if( "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" )
            set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcolor-diagnostics" )
        endif()
    endif()

endif( WIN32 )

list(APPEND LEVEL_DB_SOURCES "${LEVELDB_PORT_FILE}")
add_library( leveldb ${LEVEL_DB_SOURCES} )
target_link_libraries( leveldb ${LEVELDB_BUILD_LIBRARIES} )
target_include_directories( leveldb PRIVATE ${LEVELDB_BUILD_PRIVATE_INCLUDES}
                                    PUBLIC "${LEVEL_DB_DIR}/include" )
set_target_properties(leveldb PROPERTIES COMPILE_DEFINITIONS "${LEVELDB_BUILD_DEFINES}")

find_package( BerkeleyDB )

add_subdirectory( libraries )
add_subdirectory( vendor/miniupnp/miniupnpc )
add_subdirectory( programs )
add_subdirectory( tests )


if (ENABLE_INSTALLER)

set(VERSION_MAJOR 0)
set(VERSION_MINOR 9)
set(VERSION_PATCH 3)

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/web_wallet")
  MESSAGE(STATUS "Found web wallet source code, attempting to bundle it in the installer")
  find_package(Lineman)
  IF(LINEMAN_FOUND)
    MESSAGE(STATUS "Found lineman.js in ${LINEMAN_EXECUTABLE}")

    set(WEB_WALLET_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/web_wallet")
    set(WEB_WALLET_GENERATED "${WEB_WALLET_ROOT}/generated")
    file(GLOB_RECURSE WEB_WALLET_SOURCES "${WEB_WALLET_ROOT}/app" "*.css" "*.coffee" "*.png" "*.jpg" "*.html" "*.ico")

    #                  COMMAND "echo" "${NPM_EXECUTABLE}" "install"
    ADD_CUSTOM_TARGET(web_wallet_htdocs ALL
                      WORKING_DIRECTORY "${WEB_WALLET_ROOT}"
                      COMMAND "${CMAKE_COMMAND}" "-E" "remove_directory" "${WEB_WALLET_GENERATED}"
                      COMMAND "${LINEMAN_EXECUTABLE}" "build"
                      DEPENDS ${WEB_WALLET_SOURCES})
    INSTALL(DIRECTORY "${WEB_WALLET_GENERATED}/" DESTINATION htdocs)

    #add_custom_target(INSTALL_WITH_HTDOCS)
    #add_dependencies(INSTALL_WITH_HTDOCS web_wallet_htdocs INSTALL)

  ELSE(LINEMAN_FOUND)
    MESSAGE(WARNING "Unable to find lineman.js to process web wallet source, web wallet will be omitted")
  ENDIF(LINEMAN_FOUND)
endif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/web_wallet")


include(InstallRequiredSystemLibraries)

set(CPACK_OUTPUT_FILE_PREFIX ${CMAKE_BINARY_DIR}/packages)
set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/install)

SET(CPACK_PACKAGE_DIRECTORY "${CMAKE_INSTALL_PREFIX}")
set(CPACK_PACKAGE_NAME "bitshares")
set(CPACK_PACKAGE_VENDOR "Invictus Innovations, Inc.")
set(CPACK_PACKAGE_VERSION_MAJOR "${VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${VERSION_PATCH}")
set(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
set(CPACK_PACKAGE_DESCRIPTION "A client for the BitShares network")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A client for the BitShares network")
set(CPACK_PACKAGE_EXECUTABLES "Keyhotee;Keyhotee")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.md")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "BitShares ${CPACK_PACKAGE_VERSION}")

if(WIN32)
 SET(CPACK_GENERATOR "ZIP;NSIS")
 set(CPACK_PACKAGE_NAME "BitShares") # override above
 set(CPACK_NSIS_CONTACT "contact.us@invictus-innovations.com")
 set(CPACK_NSIS_EXECUTABLES_DIRECTORY .)
 set(CPACK_NSIS_PACKAGE_NAME "BitShares v${CPACK_PACKAGE_VERSION}")
 set(CPACK_NSIS_DISPLAY_NAME "${CPACK_NSIS_PACKAGE_NAME}")
 set(CPACK_NSIS_URL_INFO_ABOUT "http://bitshares.org")
 set(CPACK_NSIS_HELP_LINK "https://bitsharestalk.org/index.php?board=3.0")
 set(CPACK_NSIS_MENU_LINKS "http://bitshares.org" "bitshares.org")
 set(CPACK_NSIS_DEFINES "  !define MUI_STARTMENUPAGE_DEFAULTFOLDER \\\"BitShares\\\"")
 # it seems like windows zip files usually don't have a single directory inside them, unix tgz frequently do
 SET(CPACK_INCLUDE_TOPLEVEL_DIRECTORY 0)

 include(CPack)
endif(WIN32)

if(APPLE)
  set(CPACK_GENERATOR "DragNDrop")
  set(CPACK_PACKAGE_VENDOR "www.bitshares.org")
endif()

if(LINUX)
  # Linux gets a .tgz
  SET(CPACK_GENERATOR "TGZ")
  SET(CPACK_INCLUDE_TOPLEVEL_DIRECTORY 1)
endif(LINUX)

endif(ENABLE_INSTALLER)
