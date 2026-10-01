# Checks the structural rules of docs/ARCHITECTURE.md §1 that can be seen in the sources.
# Run by ctest:  cmake -DROOT=<repository root> -P check_layering.cmake
cmake_minimum_required(VERSION 3.28)

set(violations "")

# Fails for every file under the given directories that matches the pattern,
# except files listed after ALLOW (paths relative to the repository root).
function(forbid description pattern)
  cmake_parse_arguments(ARG "" "" "IN;ALLOW" ${ARGN})
  set(found "${violations}")
  foreach(dir IN LISTS ARG_IN)
    file(GLOB_RECURSE files RELATIVE ${ROOT} ${ROOT}/${dir}/*.hpp ${ROOT}/${dir}/*.cpp)
    foreach(file IN LISTS files)
      if(file IN_LIST ARG_ALLOW)
        continue()
      endif()
      file(STRINGS ${ROOT}/${file} hits REGEX "${pattern}")
      if(hits)
        list(GET hits 0 first)
        string(STRIP "${first}" first)
        string(APPEND found "\n  ${file}: ${description}\n      ${first}")
      endif()
    endforeach()
  endforeach()
  set(violations "${found}" PARENT_SCOPE)
endfunction()

set(core_dirs include/atpl/core src/core)
set(ui_dirs include/atpl/ui src/ui)

# core: standard library only, and nothing from the layers above it
forbid("core must not include SFML" "#[ \t]*include[ \t]*[<\"]SFML/" IN ${core_dirs})
forbid("core must not include ui or app" "#[ \t]*include[ \t]*\"(atpl/)?(ui|app)/" IN ${core_dirs})

# ui: nothing from app, no threads of its own
forbid("ui must not include app" "#[ \t]*include[ \t]*\"(atpl/)?app/" IN ${ui_dirs})
forbid("only app starts threads" "std::(thread|jthread|async)[^_a-zA-Z0-9]" IN ${ui_dirs})

# Only two files touch the window
forbid("only ui.cpp and render/renderer.cpp may use sf::RenderWindow" "RenderWindow"
  IN ${ui_dirs}
  ALLOW src/ui/ui.cpp src/ui/render/renderer.cpp
)

if(violations)
  message(FATAL_ERROR "Layering rules violated:${violations}")
endif()
message(STATUS "Layering rules hold.")
