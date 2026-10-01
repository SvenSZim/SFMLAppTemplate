cmake_minimum_required(VERSION 3.21)

include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.4.0 # Use the latest stable version
)
FetchContent_MakeAvailable(Catch2)

add_executable(unit_tests
    test_interpolated.cpp
    test_rect.cpp
    test_container.cpp
    test_widget_packing.cpp
    test_event.cpp
)

target_link_libraries(unit_tests PRIVATE Catch2::Catch2WithMain sfml_app_template)
target_compile_features(unit_tests PRIVATE cxx_std_20)

include(CTest)
include(Catch)
catch_discover_tests(unit_tests)
