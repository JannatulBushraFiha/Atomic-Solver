#pragma once

#include <vector>
#include "Item.h"
#include "BoxType.h"
#include "PackingSolution.h"

class PackingSolver {
public:
    PackingSolution solve(
        const std::vector<Item>& items,
        const std::vector<BoxType>& boxes
    );
};