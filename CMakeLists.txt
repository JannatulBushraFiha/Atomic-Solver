cmake_minimum_required(VERSION 3.15)
project(AtomicFitSolver)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_executable(solver
    src/main.cpp
    src/Item.cpp
    src/Orientation.cpp
    src/PackingSolution.cpp
    src/PackingSolver.cpp
    src/Placement.cpp
    src/Position.cpp
    src/Constraints.cpp
    src/Validator.cpp
)

# ---- tests ----
enable_testing()
file(GLOB ENGINE_SOURCES src/*.cpp)
list(FILTER ENGINE_SOURCES EXCLUDE REGEX ".*/main\\.cpp$")
file(GLOB TEST_SOURCES tests/*.cpp)
add_executable(run_tests ${TEST_SOURCES} ${ENGINE_SOURCES})
target_include_directories(run_tests PRIVATE include tests)
add_test(NAME all_tests COMMAND run_tests)