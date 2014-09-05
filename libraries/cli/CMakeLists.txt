list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules")
find_package(Curses)
find_package(Readline)

file(GLOB HEADERS "include/bts/cli/*.hpp")

add_library( bts_cli 
             cli.cpp
             pretty.cpp
             print_result.cpp
             ${HEADERS})

if (READLINE_FOUND)
  target_compile_definitions (bts_cli PRIVATE HAVE_READLINE)
  set(readline_libraries ${Readline_LIBRARY})
  if (CURSES_FOUND)
    list(APPEND readline_libraries ${CURSES_LIBRARY})
  endif()
  set(readline_includes ${Readline_INCLUDE_DIR})
endif()
target_link_libraries( bts_cli 
                       PUBLIC bts_client fc bts_db bts_rpc bts_wallet bts_blockchain leveldb ${readline_libraries})
target_include_directories( bts_cli 
                            PRIVATE "${readline_includes}"
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )
if(WIN32)
  target_compile_definitions( bts_cli PRIVATE _CRT_NONSTDC_NO_DEPRECATE )
endif(WIN32)
