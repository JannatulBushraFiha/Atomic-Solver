<<<<<<< HEAD
cmake_minimum_required(VERSION 3.20)
project(AtomicFitSolver)
set(CMAKE_CXX_STANDARD 20)
=======
cmake_minimum_required(VERSION 3.15)
project(AtomicFitSolver)

set(CMAKE_CXX_STANDARD 17)
>>>>>>> origin/jannatul-final-branch
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include_directories(include)
<<<<<<< HEAD
add_executable(PerfectFitSolver
=======

add_executable(solver
>>>>>>> origin/jannatul-final-branch
    src/main.cpp
    src/Item.cpp
    src/Orientation.cpp
    src/PackingSolution.cpp
    src/PackingSolver.cpp
<<<<<<< HEAD
    src/Dimensions.cpp
    src/Item.cpp
    src/BoxType.cpp
    src/Position.cpp
    src/Placement.cpp
    src/PackingSolution.cpp
    src/Orientation.cpp
)
=======
    src/Placement.cpp
    src/Position.cpp
    src/Constraints.cpp
    src/Validator.cpp
)
>>>>>>> origin/jannatul-final-branch
