add_executable(minimal
  main.cpp
)
target_link_libraries(minimal PRIVATE atpl::app)
atpl_target_defaults(minimal)
atpl_copy_resources(minimal)
