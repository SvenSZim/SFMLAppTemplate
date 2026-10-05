# Tens of thousands of particles on all cores: a simulation showcase on the template's public API.
add_executable(particles
  main.cpp
)
target_link_libraries(particles PRIVATE atpl::app)
atpl_target_defaults(particles)
atpl_copy_resources(particles)

# Opens a real window for a few frames, with its simulation running. Labelled "display".
if(ATPL_BUILD_TESTS)
  add_test(NAME example.particles COMMAND particles --smoke-test)
  set_tests_properties(example.particles PROPERTIES LABELS display RESOURCE_LOCK display TIMEOUT 60)
endif()
