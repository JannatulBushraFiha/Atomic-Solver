#include <iostream>
#include "Problem.h"
#include "PackingSolver.h"

int main() {
    // just some made up test data to see if this actually works
    Problem problem;
    problem.items.push_back(Item("I1", "widget-a", Dimensions(10, 10, 10), 2.0));
    problem.items.push_back(Item("I2", "widget-b", Dimensions(50, 50, 50), 20.0)); // too big, should be unplaced
    problem.items.push_back(Item("I3", "widget-c", Dimensions(5, 5, 5), 1.0));

    problem.boxes.push_back(BoxType("BOX-S", Dimensions(20, 20, 20), 10.0, 0.5));
    problem.boxes.push_back(BoxType("BOX-M", Dimensions(30, 30, 30), 25.0, 1.0));

    PackingSolver solver;
    PackingSolution result = solver.solve(problem);

    std::cout << "placed:" << std::endl;
    for (size_t i = 0; i < result.solution.size(); i++) {
        std::cout << "  " << result.solution[i].itemRef << " -> " << result.solution[i].boxRef << std::endl;
    }

    std::cout << "could not place:" << std::endl;
    for (size_t i = 0; i < result.unplacedItems.size(); i++) {
        std::cout << "  " << result.unplacedItems[i] << std::endl;
    }

    return 0;
}
