#pragma once

#include <vector>
#include "Item.hpp"
#include "BoxType.hpp"
#include "PackingSolution.hpp"

class PackingSolver {
public:
    PackingSolution solve(
        const std::vector<Item>& items,
        const std::vector<BoxType>& boxes
    );
};