# The smallest application written against the template's API.
#
# Compiled, but not linked: the API is declared and not yet implemented. An object library is
# built with every build, so the example cannot drift from the API. When the implementation is
# there (Phase 4), this becomes a normal executable:
#   add_executable(starter main.cpp)  +  atpl_copy_resources(starter)
add_library(starter_api_check OBJECT
  main.cpp
)
target_link_libraries(starter_api_check PRIVATE atpl::app)
atpl_target_defaults(starter_api_check)
