cmake_minimum_required(VERSION 3.20)
project(AtomicFitSolver)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include_directories(include)
add_executable(PerfectFitSolver
    src/main.cpp
    src/PackingSolver.cpp
    src/Dimensions.cpp
    src/Item.cpp
    src/BoxType.cpp
    src/Position.cpp
    src/Placement.cpp
    src/PackingSolution.cpp
    src/Orientation.cpp
)
