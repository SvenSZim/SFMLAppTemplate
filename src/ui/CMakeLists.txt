# atpl_ui: panels, widgets, layout, input, theme, rendering.
# Depends on atpl_core and SFML Graphics. This target must never link atpl_app.
add_library(atpl_ui STATIC
  binding/any_binding.cpp
  binding/sync.cpp
  frame_loop.cpp
  input/event.cpp
  input/input_context.cpp
  input/input_system.cpp
  input/window_events.cpp
  layout/arrange.cpp
  layout/cell.cpp
  layout/grid_packing.cpp
  layout/layout.cpp
  layout/measure_context.cpp
  layout/overlay_placement.cpp
  layout/packing.cpp
  layout/panel_placement.cpp
  layout/rules.cpp
  layout/widget_layout.cpp
  model/name_index.cpp
  model/setup.cpp
  model/store.cpp
  render/draw_list.cpp
  render/font_measurer.cpp
  render/painter.cpp
  render/panel_batch.cpp
  render/profiler.cpp
  render/renderer.cpp
  render/shapes.cpp
  render/text_cache.cpp
  render/text_layout.cpp
  theme/color.cpp
  theme/style.cpp
  theme/theme.cpp
  ui.cpp
  widgets/button.cpp
  widgets/dropdown.cpp
  widgets/graph.cpp
  widgets/panel_frame.cpp
  widgets/progress_bar.cpp
  widgets/slider.cpp
  widgets/switch.cpp
  widgets/text_input.cpp
  widgets/text_display.cpp
)
add_library(atpl::ui ALIAS atpl_ui)

target_include_directories(atpl_ui
  PUBLIC ${PROJECT_SOURCE_DIR}/include
  PRIVATE ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(atpl_ui PUBLIC atpl::core SFML::Graphics)
atpl_target_defaults(atpl_ui)

# The setup API is written with designated initializers that name only the fields that differ
# from their defaults: {.min = 0, .max = 10}. GCC and Clang warn about the fields left out when
# -Wextra is on, although every field has a default. Switch that one warning off for this
# library and for everything that uses it.
if(NOT MSVC)
  target_compile_options(atpl_ui PUBLIC -Wno-missing-field-initializers)
endif()
