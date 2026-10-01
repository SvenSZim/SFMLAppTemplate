include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

# SFML: only Graphics (which brings Window and System) is used.
# Audio and Network are switched off so their system packages are not needed.
set(SFML_BUILD_AUDIO OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_NETWORK OFF CACHE BOOL "" FORCE)

FetchContent_Declare(SFML
  GIT_REPOSITORY https://github.com/SFML/SFML.git
  GIT_TAG 3.0.x
  GIT_SHALLOW ON
  EXCLUDE_FROM_ALL
  SYSTEM
)
FetchContent_MakeAvailable(SFML)
