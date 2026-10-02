add_executable(minimal
  main.cpp
)
target_link_libraries(minimal PRIVATE atpl::app)
atpl_target_defaults(minimal)
atpl_copy_resources(minimal)

# Opens a real window for a few frames. Tests labelled "display" need a display;
# on a machine without one, run them under xvfb-run or leave them out with `ctest -LE display`.
if(ATPL_BUILD_TESTS)
  add_test(NAME example.minimal COMMAND minimal --smoke-test)
  set_tests_properties(example.minimal PROPERTIES LABELS display RESOURCE_LOCK display TIMEOUT 60)
endif()
