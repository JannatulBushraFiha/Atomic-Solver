cmake_minimum_required(VERSION 3.15)
project(AtomicFitSolver)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    include
    lib
    )

add_executable(AtomicFitSolver
    src/main.cpp
    src/Item.cpp
    src/Orientation.cpp
    src/PackingSolution.cpp
    src/PackingSolver.cpp
    src/Placement.cpp
    src/Position.cpp
    src/server.cpp
)

if(WIN32)
    target_link_libraries(AtomicFitSolver PRIVATE ws2_32 wsock32)
endif()