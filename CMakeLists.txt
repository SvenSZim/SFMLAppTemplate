cmake_minimum_required(VERSION 3.28)
project(SFMLAppTemplate VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(ATPL_BUILD_TESTS "Build the unit tests" ${PROJECT_IS_TOP_LEVEL})
option(ATPL_BUILD_EXAMPLES "Build the example applications" ${PROJECT_IS_TOP_LEVEL})
option(ATPL_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(ATPL_LINK_COMPILE_COMMANDS "Link compile_commands.json into the source directory" ${PROJECT_IS_TOP_LEVEL})

# Editors and language servers look for compile_commands.json in the source directory,
# but CMake writes it into the build directory. Link it where they find it.
if(ATPL_LINK_COMPILE_COMMANDS AND NOT WIN32)
  file(CREATE_LINK
    ${CMAKE_BINARY_DIR}/compile_commands.json
    ${CMAKE_SOURCE_DIR}/compile_commands.json
    SYMBOLIC
  )
endif()

include(cmake/warnings.cmake)
include(cmake/dependencies.cmake)
include(cmake/resources.cmake)

# Layers, lowest first. Dependencies point one way only: app -> ui -> core.
add_subdirectory(src/core)
add_subdirectory(src/ui)
add_subdirectory(src/app)

if(ATPL_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(ATPL_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
