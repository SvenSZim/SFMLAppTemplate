add_executable(showcase
  main.cpp
)
target_link_libraries(showcase PRIVATE atpl::app)
atpl_target_defaults(showcase)
atpl_copy_resources(showcase)

# Opens a real window for a few frames. Tests labelled "display" need a display;
# on a machine without one, run them under xvfb-run or leave them out with `ctest -LE display`.
if(ATPL_BUILD_TESTS)
  add_test(NAME example.showcase COMMAND showcase --smoke-test)
  set_tests_properties(example.showcase PROPERTIES LABELS display RESOURCE_LOCK display TIMEOUT 60)
endif()
