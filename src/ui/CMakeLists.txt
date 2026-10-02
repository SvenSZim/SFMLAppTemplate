# atpl_ui: panels, widgets, layout, input, theme, rendering.
# Depends on atpl_core and SFML Graphics. This target must never link atpl_app.
add_library(atpl_ui STATIC
  layout/packing.cpp
  render/shapes.cpp
  theme/color.cpp
  theme/theme.cpp
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
