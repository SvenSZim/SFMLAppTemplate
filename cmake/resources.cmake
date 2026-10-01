# Copies the repository's resources/ directory next to the executables, where
# atpl::Resources::nextToExecutable() looks for it.
#
# All executables of this project are written to CMAKE_RUNTIME_OUTPUT_DIRECTORY, so there is one
# copy, made by one target. It runs on every build and only touches files that changed.
get_property(atpl_is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(atpl_is_multi_config)
  set(atpl_resources_destination ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/resources)
else()
  set(atpl_resources_destination ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/resources)
endif()

add_custom_target(atpl_resources
  COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
          ${PROJECT_SOURCE_DIR}/resources
          ${atpl_resources_destination}
  COMMENT "Copying resources next to the executables"
  VERBATIM
)

# Makes sure the resources are in place whenever the target is built.
function(atpl_copy_resources target)
  add_dependencies(${target} atpl_resources)
endfunction()
