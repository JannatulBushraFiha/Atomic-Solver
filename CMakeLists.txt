cmake_minimum_required(VERSION 3.20)

project(AtomicFitSolver)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

set(SOLVER_SOURCES
    src/Orientation.cpp
    src/PackingSolver.cpp
    src/Constraints.cpp
    src/Validator.cpp
)

add_executable(solver src/main.cpp ${SOLVER_SOURCES})
add_executable(integrationTest tests/integrationTest.cpp ${SOLVER_SOURCES})
add_executable(unitTests
    tests/test_main.cpp
    tests/test_constraints.cpp
    tests/test_edge_cases.cpp
    tests/test_solver.cpp
    tests/test_validator.cpp
    ${SOLVER_SOURCES}
)
target_compile_definitions(unitTests PRIVATE SOLVER_EXECUTABLE_PATH="$<TARGET_FILE:solver>")
add_dependencies(unitTests solver)

enable_testing()
add_test(NAME integrationTest COMMAND integrationTest)
add_test(NAME unitTests COMMAND unitTests)
