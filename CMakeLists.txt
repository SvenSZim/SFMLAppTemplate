cmake_minimum_required(VERSION 3.28)
project(SFMLAppTemplate LANGUAGES CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

FetchContent_Declare(SFML
  GIT_REPOSITORY https://github.com/SFML/SFML.git
  GIT_TAG 3.0.x
  GIT_SHALLOW ON
  EXCLUDE_FROM_ALL
  SYSTEM
)
FetchContent_MakeAvailable(SFML)

include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

# Automatically adds all cpp files contained in the src directory
file(GLOB_RECURSE source_files src/*.cpp)

set(SOURCES ${source_files})

add_executable(app ${SOURCES})
target_include_directories(app PRIVATE "src")
target_link_libraries(app PRIVATE SFML::Graphics)
target_compile_features(app PRIVATE cxx_std_17)
