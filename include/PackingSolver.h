#pragma once

#include <vector>
#include "Item.h"
#include "BoxType.h"
#include "PackingSolution.h"
#include "Constraints.h"

class PackingSolver {
public:
    Constraints constraints;

    PackingSolution solve(
        const std::vector<Item>& items,
        const std::vector<BoxType>& boxes
    );
};