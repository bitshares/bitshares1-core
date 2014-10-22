add_subdirectory( client )
add_subdirectory( utils )

# the following includes QT Wallet into the build (optional)
set(INCLUDE_VOTING_GUI FALSE CACHE BOOL "Include Voting GUI")
if(${INCLUDE_VOTING_GUI})
  add_subdirectory( voting_gui "${CMAKE_BINARY_DIR}/programs/voting_gui" )
endif()
