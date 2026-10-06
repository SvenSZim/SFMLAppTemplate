# Copies resources next to the executables, where atpl::Resources::nextToExecutable() looks for
# them: the template's own (its fonts and textures), and an application's.
#
# The template's own executables are all written to CMAKE_RUNTIME_OUTPUT_DIRECTORY, so they share
# one copy, made by one target. It runs on every build and only touches files that changed.
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

# Where the template is, for the function below, which an application calls from its own
# directory (where variables of this scope are not seen).
set_property(GLOBAL PROPERTY ATPL_SOURCE_DIR ${PROJECT_SOURCE_DIR})

# Makes sure the resources are next to `target` whenever it is built.
#
# For an application that uses the template (through FetchContent, see new-project/): the
# template's resources, then the application's own `resources` directory next to its
# CMakeLists.txt, if there is one, are copied next to the executable. A file of the
# application's replaces the template's of the same name.
function(atpl_copy_resources target)
  get_property(template GLOBAL PROPERTY ATPL_SOURCE_DIR)
  # By the project that calls, not by where its files are: new-project/ lies inside the
  # template's directory, but is a project of its own.
  if(PROJECT_SOURCE_DIR STREQUAL template)
    add_dependencies(${target} atpl_resources) # one of the template's own: the shared copy
    return()
  endif()

  set(destination $<TARGET_FILE_DIR:${target}>/resources)
  set(commands COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${template}/resources ${destination})
  if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    list(APPEND commands
      COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${CMAKE_CURRENT_SOURCE_DIR}/resources ${destination})
  endif()
  add_custom_target(${target}_resources ${commands}
    COMMENT "Copying resources next to ${target}"
    VERBATIM
  )
  add_dependencies(${target} ${target}_resources)
endfunction()
