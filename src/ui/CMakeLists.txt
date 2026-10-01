# atpl_ui: panels, widgets, layout, input, theme, rendering.
# Depends on atpl_core and SFML Graphics. This target must never link atpl_app.
add_library(atpl_ui STATIC
  placeholder.cpp
)
add_library(atpl::ui ALIAS atpl_ui)

target_include_directories(atpl_ui
  PUBLIC ${PROJECT_SOURCE_DIR}/include
  PRIVATE ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(atpl_ui PUBLIC atpl::core SFML::Graphics)
atpl_target_defaults(atpl_ui)
