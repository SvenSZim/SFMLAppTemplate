# Pathfinding on a grid, step by step: an algorithm showcase on the template's public API.
add_executable(pathfinding
  main.cpp
)
target_link_libraries(pathfinding PRIVATE atpl::app)
atpl_target_defaults(pathfinding)
atpl_copy_resources(pathfinding)

# Opens a real window for a few frames, with its search running. Labelled "display".
if(ATPL_BUILD_TESTS)
  add_test(NAME example.pathfinding COMMAND pathfinding --smoke-test)
  set_tests_properties(example.pathfinding PROPERTIES LABELS display RESOURCE_LOCK display TIMEOUT 60)
endif()
