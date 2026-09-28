cmake_minimum_required(VERSION 3.28)
project(SFMLAppTemplate LANGUAGES CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(ENABLE_UNIT_TESTS "Build unit tests" ON)

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

# Automatically adds all cpp files contained in the src directory
file(GLOB_RECURSE source_files src/*.cpp)

set(APP_TEMPLATE_SOURCES ${source_files})
list(REMOVE_ITEM APP_TEMPLATE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

add_library(sfml_app_template ${APP_TEMPLATE_SOURCES})
target_include_directories(sfml_app_template PUBLIC "include")
target_link_libraries(sfml_app_template PUBLIC SFML::Graphics)
target_compile_features(sfml_app_template PUBLIC cxx_std_17)

add_executable(app src/main.cpp)
target_link_libraries(app PRIVATE sfml_app_template)
target_compile_features(app PRIVATE cxx_std_17)

enable_testing()
if(ENABLE_UNIT_TESTS)
  add_subdirectory(tests)
endif()
