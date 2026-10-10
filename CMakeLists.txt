cmake_minimum_required(VERSION 3.20)

project(AtomicFitSolver)

set(CMAKE_CXX_STANDARD 20)
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

add_executable(integrationTest
    src/integrationTest.cpp
    src/Item.cpp
    src/Orientation.cpp
    src/PackingSolution.cpp
    src/PackingSolver.cpp
    src/Placement.cpp
    src/Position.cpp
    src/Constraints.cpp
    src/Validator.cpp
)

enable_testing()
add_test(NAME integrationTest COMMAND integrationTest)
