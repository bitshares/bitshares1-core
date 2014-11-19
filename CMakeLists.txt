# Defines BitShares library target.
project( BitShares )
cmake_minimum_required( VERSION 2.8.12 )

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

# Warning Boost libraries must be detected before processing leveldb cmake (included below) since
# Boost_Include_Dir variable is referenced there.

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

set(Boost_USE_STATIC_LIBS ON)
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


if( WIN32 )
    message( STATUS "Configuring BitShares on WIN32")
    set( DB_VERSION 60 )
    set( BDB_STATIC_LIBS 1 )

    set( ZLIB_LIBRARIES "" )
    set( LEVEL_DB_DIR vendor/leveldb-win )
    add_subdirectory( "${LEVEL_DB_DIR}"    )

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
else( WIN32 )
    # Apple AND Linux Options Here
    find_package( ZLIB REQUIRED )

    #set( LEVEL_DB_DIR "${CMAKE_SOURCE_DIR}/libraries/leveldb" )
    #include( ExternalProject )
    #ExternalProject_Add( LevelDB
    #PREFIX "${LEVEL_DB_DIR}"
    #SOURCE_DIR "${LEVEL_DB_DIR}"
    #BUILD_IN_SOURCE 1
    #CONFIGURE_COMMAND ""
    #BUILD_COMMAND "$(MAKE)"
    #INSTALL_COMMAND ""
    #)
    #include_directories( "${LEVEL_DB_DIR}" "${LEVEL_DB_DIR}/include" )
    #add_library( leveldb STATIC IMPORTED )
    #add_dependencies( leveldb LevelDB )
    #set_property(TARGET leveldb PROPERTY IMPORTED_LOCATION "${LEVEL_DB_DIR}/libleveldb.a" )

    set( LEVEL_DB_DIR vendor/leveldb-1.12.0 )
    add_subdirectory( "${LEVEL_DB_DIR}"    )

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
        set( CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++11 -Wall -Wstack-usage=6144" )
        set( rt_library rt )
        set( pthread_library pthread)
        set( crypto_library crypto)
        if ( FULL_STATIC_BUILD )
          set( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -static-libgcc")
        endif ( FULL_STATIC_BUILD )
    endif( APPLE )

    if( "${CMAKE_GENERATOR}" STREQUAL "Ninja" )
        if( "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" )
            set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcolor-diagnostics" )
        endif()
    endif()

	set(Boost_USE_STATIC_LIBS ON)
endif( WIN32 )

find_package( BerkeleyDB )

set( CUSTOM_URL_SCHEME "bts" )

add_subdirectory( libraries )
add_subdirectory( vendor/miniupnp/miniupnpc )
add_subdirectory( programs )
add_subdirectory( tests )


if (ENABLE_INSTALLER)

set(VERSION_MAJOR 0)
set(VERSION_MINOR 1)
set(VERSION_PATCH 0)

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
