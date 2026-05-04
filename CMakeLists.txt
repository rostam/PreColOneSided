cmake_minimum_required(VERSION 3.20)
project(PreColOneSided LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wextra -Wpedantic)

# Core library
add_library(precol_lib
    src/mtx_reader.cpp
    src/bipartite_graph.cpp
    src/coloring.cpp
)
target_include_directories(precol_lib PUBLIC src)

# Main executable
add_executable(precol src/main.cpp)
target_link_libraries(precol PRIVATE precol_lib)

# Tests via Catch2
include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
)
FetchContent_MakeAvailable(Catch2)

add_executable(tests
    tests/test_mtx_reader.cpp
    tests/test_bipartite_graph.cpp
    tests/test_coloring.cpp
)
target_link_libraries(tests PRIVATE precol_lib Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(tests)
