#pragma once

#include <vector>
#include "Placement.h"

struct PackingSolution {
    std::vector<Placement> placements;
    std::vector<std::string> unplacedItems;
};