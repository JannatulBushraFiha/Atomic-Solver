cmake_minimum_required(VERSION 3.15)
project(AtomicFitSolver)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_executable(solver
    src/main.cpp
    src/Orientation.cpp
    src/PackingSolver.cpp
    src/Constraints.cpp
    src/Validator.cpp
)