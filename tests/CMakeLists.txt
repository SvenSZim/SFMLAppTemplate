include(FetchContent)
FetchContent_Declare(Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.4.0
  GIT_SHALLOW ON
  EXCLUDE_FROM_ALL
  SYSTEM
)
FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

# One test executable per layer. Tests may include internal headers from src/.
function(atpl_add_tests layer)
  set(target atpl_${layer}_tests)
  add_executable(${target} ${ARGN})
  target_link_libraries(${target} PRIVATE atpl::${layer} Catch2::Catch2WithMain)
  target_include_directories(${target} PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/tests)
  atpl_target_defaults(${target})
  catch_discover_tests(${target} TEST_PREFIX "${layer}.")
endfunction()

atpl_add_tests(core
  core/test_easing.cpp
  core/test_interpolated.cpp
  core/test_version.cpp
)
atpl_add_tests(ui
  ui/test_packing.cpp
  ui/test_rect.cpp
)
atpl_add_tests(app
  app/test_resources.cpp
)
# The resource tests load the real font, so they need the resources next to their executable.
atpl_copy_resources(atpl_app_tests)

# The layer and ownership rules that can be checked by looking at the sources.
add_test(NAME "structure.layering"
  COMMAND ${CMAKE_COMMAND} -DROOT=${PROJECT_SOURCE_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/check_layering.cmake
)
