list( APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/libraries/fc/GitVersionGen" )
include( GetGitRevisionDescription )
get_git_head_revision(GIT_REFSPEC BITSHARES_TOOLKIT_GIT_REVISION_SHA)
get_git_unix_timestamp(BITSHARES_TOOLKIT_GIT_REVISION_UNIX_TIMESTAMP)

file(GLOB headers "include/bts/utilities/*.hpp")

set(sources key_conversion.cpp string_escape.cpp
            ${headers})

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/git_revision.cpp.in" "${CMAKE_CURRENT_BINARY_DIR}/git_revision.cpp" @ONLY)
list(APPEND sources "${CMAKE_CURRENT_BINARY_DIR}/git_revision.cpp")

add_library( bts_utilities 
             ${sources}
             ${HEADERS} )
target_link_libraries( bts_utilities fc )
target_include_directories( bts_utilities 
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )
if (USE_PCH)
  set_target_properties(bts_utilities PROPERTIES COTIRE_ADD_UNITY_BUILD FALSE)
  cotire(bts_utilities)
endif(USE_PCH)