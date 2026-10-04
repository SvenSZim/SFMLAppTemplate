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

# Test executables are asked for their tests when ctest runs, not right after they are built:
# a ThreadSanitizer build can only start where address randomisation is reduced (see the CI
# workflow), which is arranged for running the tests, not for building them.
if(ATPL_SANITIZE MATCHES "thread")
  set(CMAKE_CATCH_DISCOVER_TESTS_DISCOVERY_MODE PRE_TEST)
endif()

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

# Tests of a layer that draw and look at the result. They need a display (a virtual one will
# do) and carry the label "display": `ctest -LE display` leaves them out.
# They never run at the same time, even with `ctest -j`: windows that open side by side take the
# focus from each other, which asks for frames the tests do not expect, and disturb each
# other's timing.
function(atpl_add_display_tests layer)
  set(target atpl_${layer}_display_tests)
  add_executable(${target} ${ARGN})
  target_link_libraries(${target} PRIVATE atpl::${layer} Catch2::Catch2WithMain)
  target_include_directories(${target} PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/tests)
  atpl_target_defaults(${target})
  catch_discover_tests(${target}
    TEST_PREFIX "${layer}.display."
    WORKING_DIRECTORY $<TARGET_FILE_DIR:${target}>
    PROPERTIES LABELS display RESOURCE_LOCK display
  )
endfunction()

atpl_add_tests(core
  core/test_easing.cpp
  core/test_grid.cpp
  core/test_interpolated.cpp
  core/test_param.cpp
  core/test_queue.cpp
  core/test_random.cpp
  core/test_series.cpp
  core/test_snapshot.cpp
  core/test_text_log.cpp
  core/test_thread_pool.cpp
  core/test_timing.cpp
  core/test_version.cpp
)
atpl_add_tests(ui
  ui/test_basic_widgets.cpp
  ui/test_binding.cpp
  ui/test_bindings.cpp
  ui/test_card_stack.cpp
  ui/test_cell.cpp
  ui/test_display_widgets.cpp
  ui/test_input_widgets.cpp
  ui/test_event.cpp
  ui/test_frame.cpp
  ui/test_input.cpp
  ui/test_grid_packing.cpp
  ui/test_layout_theme.cpp
  ui/test_log.cpp
  ui/test_model.cpp
  ui/test_name_index.cpp
  ui/test_packing.cpp
  ui/test_painter.cpp
  ui/test_panel_frame.cpp
  ui/test_panel_placement.cpp
  ui/test_panel_sizing.cpp
  ui/test_paragraph.cpp
  ui/test_panel_batch.cpp
  ui/test_profiler.cpp
  ui/test_rect.cpp
  ui/test_scrolling.cpp
  ui/test_shapes.cpp
  ui/test_text_display.cpp
  ui/test_text_layout.cpp
  ui/test_theme.cpp
  ui/test_theme_resolve.cpp
  ui/test_widget_layout.cpp
  ui/test_window_events.cpp
)
atpl_add_tests(app
  app/test_resources.cpp
  app/test_simulation_runner.cpp
)
atpl_add_display_tests(ui
  ui/display/test_frame_loop.cpp
  ui/display/test_profiler_overlay.cpp
  ui/display/test_renderer.cpp
  ui/display/test_text.cpp
  ui/display/test_ui.cpp
)
# The text tests load the real font.
atpl_copy_resources(atpl_ui_display_tests)
# The resource tests load the real font, so they need the resources next to their executable.
atpl_copy_resources(atpl_app_tests)
atpl_add_display_tests(app
  app/display/test_app.cpp
  app/display/test_camera.cpp
)
atpl_copy_resources(atpl_app_display_tests)

# Measures the render pipeline on a scene of a hundred widgets; see docs/PROJECT_PLAN.md 5.5.
# As a test it runs for a few frames only: the times mean nothing then, but it checks that an
# idle scene draws no frames and that shapes take at most two draw calls per panel.
add_executable(atpl_render_bench bench/render_bench.cpp)
target_link_libraries(atpl_render_bench PRIVATE atpl::app)
target_include_directories(atpl_render_bench PRIVATE ${PROJECT_SOURCE_DIR}/src)
atpl_target_defaults(atpl_render_bench)
atpl_copy_resources(atpl_render_bench)
add_test(NAME bench.render COMMAND atpl_render_bench --frames 20 --idle-ms 300)
set_tests_properties(bench.render PROPERTIES LABELS display RESOURCE_LOCK display TIMEOUT 60)

# Checks that the UI keeps its pace while every simulation tick takes long; see docs/PROJECT_PLAN.md
# 5.5. As a test it runs for a few seconds only, and judges only that the UI kept going.
add_executable(atpl_responsiveness_bench bench/responsiveness_bench.cpp)
target_link_libraries(atpl_responsiveness_bench PRIVATE atpl::app)
atpl_target_defaults(atpl_responsiveness_bench)
atpl_copy_resources(atpl_responsiveness_bench)
add_test(NAME bench.responsiveness COMMAND atpl_responsiveness_bench --seconds 3 --tick-ms 1000)
set_tests_properties(bench.responsiveness PROPERTIES LABELS display RESOURCE_LOCK display TIMEOUT 60)

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
  api/app_usage.cpp
  api/core_usage.cpp
  api/event_usage.cpp
  api/ui_usage.cpp
  api/widget_usage.cpp
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
