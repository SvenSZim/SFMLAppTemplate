# A new application on SFMLAppTemplate. Copy this directory, rename the project and the
# executable, and build:
#
#   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build --parallel
#   build/my_application
#
# The template, and SFML with it, is downloaded at the version below while CMake configures. To
# build against a local copy of the template instead (to try changes to it), pass
# -DFETCHCONTENT_SOURCE_DIR_SFMLAPPTEMPLATE=/path/to/SFMLAppTemplate.
cmake_minimum_required(VERSION 3.28)
project(MyApplication LANGUAGES CXX)

include(FetchContent)
FetchContent_Declare(SFMLAppTemplate
  GIT_REPOSITORY https://github.com/SvenSZim/SFMLAppTemplate.git
  GIT_TAG v0.1.0
  GIT_SHALLOW ON
)
FetchContent_MakeAvailable(SFMLAppTemplate) # the libraries atpl::core, atpl::ui, atpl::app

add_executable(my_application
  src/main.cpp
)
target_link_libraries(my_application PRIVATE atpl::app)

# The template's fonts, and this project's resources/ directory, next to the executable.
atpl_copy_resources(my_application)

# `ctest` opens the window for a few frames and closes it again: a check that it starts.
enable_testing()
add_test(NAME smoke COMMAND my_application --smoke-test)
