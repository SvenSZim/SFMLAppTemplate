# atpl_core: threading, parameters, timing, utilities.
# Standard library only. This target must never link SFML.
add_library(atpl_core STATIC
  easing.cpp
  version.cpp
)
add_library(atpl::core ALIAS atpl_core)

target_include_directories(atpl_core
  PUBLIC ${PROJECT_SOURCE_DIR}/include
  PRIVATE ${PROJECT_SOURCE_DIR}/src
)
target_compile_definitions(atpl_core PRIVATE
  ATPL_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
  ATPL_VERSION_MINOR=${PROJECT_VERSION_MINOR}
  ATPL_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)
atpl_target_defaults(atpl_core)
