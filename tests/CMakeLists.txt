include(FetchContent)
FetchContent_Declare(Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.4.0
  GIT_SHALLOW ON
  EXCLUDE_FROM_ALL
  SYSTEM
)
FetchContent_MakeAvailable(Catch2)
# Catch2 must be built with the same C++ standard as the tests. Otherwise parts of it that depend
# on the standard (printing std::string_view, for one) are missing at link time, as seen with MSVC.
target_compile_features(Catch2 PUBLIC cxx_std_20)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

# One test executable per layer. Tests may include internal headers from src/.
function(atpl_add_tests layer)
  set(target atpl_${layer}_tests)
  add_executable(${target} ${ARGN})
  target_link_libraries(${target} PRIVATE atpl::${layer} Catch2::Catch2WithMain)
  target_include_directories(${target} PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/tests)
  atpl_target_defaults(${target})
  # Run from the executable's directory: resources are next to it, and on platforms where the
  # executable cannot be located any other way, the working directory is the fallback.
  catch_discover_tests(${target}
    TEST_PREFIX "${layer}."
    WORKING_DIRECTORY $<TARGET_FILE_DIR:${target}>
  )
endfunction()

atpl_add_tests(core
  core/test_easing.cpp
  core/test_interpolated.cpp
  core/test_version.cpp
)
atpl_add_tests(ui
  ui/test_binding.cpp
  ui/test_event.cpp
  ui/test_packing.cpp
  ui/test_rect.cpp
)
atpl_add_tests(app
  app/test_resources.cpp
)
# The resource tests load the real font, so they need the resources next to their executable.
atpl_copy_resources(atpl_app_tests)

# Every public header must compile when it is the only thing included, and the usage shown in
# tests/api must keep compiling. Both are checked by building, not by running: this target is
# part of every build and is never linked.
#
# The header list is globbed on purpose, unlike target source lists: the point is to catch the
# header somebody forgot to list.
file(GLOB_RECURSE atpl_public_headers CONFIGURE_DEPENDS
  RELATIVE ${PROJECT_SOURCE_DIR}/include
  ${PROJECT_SOURCE_DIR}/include/atpl/*.hpp
)
set(atpl_header_check_sources "")
foreach(header IN LISTS atpl_public_headers)
  string(MAKE_C_IDENTIFIER ${header} header_id)
  set(check_source ${CMAKE_CURRENT_BINARY_DIR}/header_check/${header_id}.cpp)
  file(CONFIGURE OUTPUT ${check_source} CONTENT "#include \"${header}\"\n")
  list(APPEND atpl_header_check_sources ${check_source})
endforeach()

add_library(atpl_api_check OBJECT
  ${atpl_header_check_sources}
  api/core_usage.cpp
  api/event_usage.cpp
  api/ui_usage.cpp
)
target_link_libraries(atpl_api_check PRIVATE atpl::app)
atpl_target_defaults(atpl_api_check)

# Code the API must reject. Each file is built by a test that passes only if the build fails.
# The files are kept minimal, and each has a counterpart in api/ui_usage.cpp that does compile,
# so a failure can only come from the line under test.
function(atpl_must_not_compile name)
  set(target atpl_must_not_compile_${name})
  add_library(${target} OBJECT EXCLUDE_FROM_ALL api/must_not_compile/${name}.cpp)
  target_link_libraries(${target} PRIVATE atpl::app)
  atpl_target_defaults(${target})
  add_test(NAME api.must_not_compile.${name}
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ${target} --config $<CONFIG>
  )
  set_tests_properties(api.must_not_compile.${name} PROPERTIES WILL_FAIL TRUE)
endfunction()

atpl_must_not_compile(bind_plain_variable)
atpl_must_not_compile(describe_plain_variable)
atpl_must_not_compile(describe_wrong_kind)

# The layer and ownership rules that can be checked by looking at the sources.
add_test(NAME "structure.layering"
  COMMAND ${CMAKE_COMMAND} -DROOT=${PROJECT_SOURCE_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/check_layering.cmake
)
