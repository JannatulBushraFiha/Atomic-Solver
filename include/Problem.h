#ifndef PROBLEM_H
#define PROBLEM_H

#include <vector>
#include "Item.h"
#include "BoxType.h"

class Problem {
public:
    std::vector<Item> items;
    std::vector<BoxType> boxes;
};

#endif
