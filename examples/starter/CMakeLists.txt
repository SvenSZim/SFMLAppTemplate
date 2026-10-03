# The smallest application written against the template's API: how an application is meant to read.
add_executable(starter
  main.cpp
)
target_link_libraries(starter PRIVATE atpl::app)
atpl_target_defaults(starter)
atpl_copy_resources(starter)

# Opens a real window for a few frames, with its simulation running. Labelled "display".
if(ATPL_BUILD_TESTS)
  add_test(NAME example.starter COMMAND starter --smoke-test)
  set_tests_properties(example.starter PROPERTIES LABELS display RESOURCE_LOCK display TIMEOUT 60)
endif()
