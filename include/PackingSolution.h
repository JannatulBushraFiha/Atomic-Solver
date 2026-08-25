#pragma once

#include <vector>
#include <string>
#include "Placement.h"

struct UsedBox {
    std::string boxReference;
    int boxInstance;
    double totalWeight = 0.0;
};

struct PackingSolution {
    std::vector<Placement> placements;
    std::vector<std::string> unplacedItems;
    std::vector<UsedBox> usedBoxes;
};