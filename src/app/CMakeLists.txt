# atpl_app: window, main loop, simulation thread, optional helpers.
# The only layer that starts threads.
add_library(atpl_app STATIC
  app.cpp
  executable_path.cpp
  simulation_runner.cpp
  resources.cpp
)
add_library(atpl::app ALIAS atpl_app)

target_include_directories(atpl_app
  PUBLIC ${PROJECT_SOURCE_DIR}/include
  PRIVATE ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(atpl_app PUBLIC atpl::ui)
atpl_target_defaults(atpl_app)
