# Defines bitshares library target.
project( BitShares )
cmake_minimum_required( VERSION 2.8.12 )

list( APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules" )
list( APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/libraries/fc/CMakeModules" )
list( APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/libraries/fc/GitVersionGen" )
include( GetGitRevisionDescription )
get_git_head_revision( GIT_REFSPEC GIT_SHA2 )


if( WIN32 )
    message( STATUS "Configuring Bitshares on WIN32")
    set( DB_VERSION 60 )
    set( BDB_STATIC_LIBS 1 )

    set( ZLIB_LIBRARIES "" )
    set( LEVEL_DB_DIR vendor/leveldb-win )

    set(BOOST_ROOT $ENV{BOOST_ROOT})
    set(Boost_USE_DEBUG_PYTHON ON)
    set(Boost_USE_MULTITHREADED ON)
    set(Boost_USE_STATIC_LIBS OFF)
    set(BOOST_ALL_DYN_LINK ON) # force dynamic linking for all libraries
    SET( DEFAULT_EXECUTABLE_INSTALL_DIR bin/ )
	
     set( PLATFORM_SPECIFIC_LIBS WS2_32.lib iphlpapi.lib )
     add_definitions(-DBOOST_TEST_DYN_LINK)
     set(CRYPTO_LIB)
else( WIN32 )
    # Apple AND Linux Options Here
    find_package( ZLIB REQUIRED )
    set( LEVEL_DB_DIR vendor/leveldb-1.12.0 )

    find_library(READLINE_LIBRARIES NAMES readline)
    find_path(READLINE_INCLUDE_DIR readline/readline.h)
    if(NOT READLINE_INCLUDE_DIR OR NOT READLINE_LIBRARIES)
      MESSAGE(FATAL_ERROR "Could not find lib readline.")
    endif()
    
    if( APPLE )
        # Apple Specific Options Here
        message( STATUS "Configuring Bitshares on OS X" )
        set( CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++11 -stdlib=libc++  -Wall -ferror-limit=4" )
    else( APPLE ) 
        # Linux Specific Options Here
        message( STATUS "Configuring Bitshares on Linux" )
        set( CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++11 -Wall " )
        set( rt_library rt )
        set( pthread_library pthread)
        set( crypto_library crypto)
    endif( APPLE )
	set(Boost_USE_STATIC_LIBS ON)
endif( WIN32 )

find_package( BerkeleyDB )

find_package( Boost 1.53 REQUIRED COMPONENTS 
                   thread 
                   date_time
                   system 
                   filesystem 
                   program_options 
                   serialization 
                   chrono 
                   unit_test_framework 
                   context 
                   locale)
  # For Boost 1.53 on windows, coroutine was not in BOOST_LIBRARYDIR and do not need it to build,  but if boost versin >= 1.54, find coroutine otherwise will cause link errors
  IF(NOT "${Boost_VERSION}" MATCHES "1.53(.*)")
    SET(BOOST_LIBRARIES_TEMP ${Boost_LIBRARIES})
    FIND_PACKAGE(Boost 1.54 REQUIRED COMPONENTS coroutine)
    SET(Boost_LIBRARIES ${BOOST_LIBRARIES_TEMP} ${Boost_LIBRARIES})
  ENDIF()


include_directories( libraries/fc/include )
include_directories( libraries/blockchain/include )
include_directories( ${Boost_INCLUDE_DIR} )

add_subdirectory( libraries )
add_subdirectory( "${LEVEL_DB_DIR}"    )
add_subdirectory( vendor/miniupnp/miniupnpc )
add_subdirectory( programs )
add_subdirectory( tests )
#add_subdirectory( programs  )
